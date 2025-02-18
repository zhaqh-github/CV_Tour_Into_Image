#include <GL/glut.h>
#include <GL/glui.h>
#include <iostream>
#include <cmath>

#include <vector>
#include <map>
#include <opencv2/opencv.hpp>

#include "inc/inpaint/criminisi_inpainter.h"

using namespace std;

//////////////////////////////////////////////////////////
/* Tour Into the Picture, Copyright 2002 Ryoichi Mizuno */
/* ryoichi[at]mizuno.org                                */
/* Dept. of Complexity Science and Engineering          */
/* at The University of Tokyo                           */
//////////////////////////////////////////////////////////

struct vector2D // 2次元ベクトルの構造体
{
	float sx; // screen座標系のx座標
	float sy; // screen座標系のy座標
};

struct vector5D // 5次元ベクトルの構造体
{
	float x; // world座標系のx座標
	float y; // world座標系のy座標
	float z; // world座標系のz座標
	float sx; // screen座標系のx座標
	float sy; // screen座標系のy座標
};

struct vector3D // 3次元ベクトルの構造体
{
	float x; // world座標系のx座標
	float y; // world座標系のy座標
	float z; // world座標系のz座標
};

struct color3D // 色の構造体
{
	float r; // 赤
	float g; // 緑
	float b; // 青
};

struct ImageInfo {
	cv::Mat originalImage;
	cv::Mat targetMask;
    cv::Mat sourceMask;
    cv::Mat background;
};

/* 数学関係の定数 */
const float PI = 3.141592853f;

/* ファイルハンドリング関係の変数 */
const int maxnumberoffiles = 100; // 最大のファイル数
int currentfilenumber = 0; // 現在選択されているファイルの番号
typedef char FName_t[260]; // 長さ260の文字配列型
FName_t bmpfilename[maxnumberoffiles]; // ファイル名の配列（最大100）

									   /* bmpファイルハンドリング関係の変数 */
GLuint texture; // 元画像のデータへのポインタ
							  /* 3次元背景のモデリング関係の変数 */
vector2D ctrlPoint[5]; // 制御点のスクリーン座標
int selectedCtrlPointIndex = -1; // 選択された制御点のインデックス
float gradient[4]; // スパイダリーメッシュの傾き
struct vector5D estimatedVertex[13]; // 推定された頂点の座標

struct vector3D eye; // 視点のワールド座標
int resolutionnumber = 0; // 現在選択されている解像度の番号
int modenumber = 0;
bool on_press = false; //whether drawing the fg contour
float rough_coefficient = 0.05f; // 画像解析の細かさ（preview: 0.05f, lowresolution: 0.01f, normal:0.005, highresolution: 0.001f）
int deduceFlag = 0; // 現在の処理を示す旗
bool test_flag = false;
					/* 透視投影関係の変数 */
float theta = 0.0f; // 方位角
float phi = 0.0f; // 仰角
float R, R0; // 視点と注視点の距離
float mat[9]; // 変換行列
float pbeginsx, pbeginsy; // マウスドラッグの始点
vector2D transferedEstimatedVertex[13]; // 透視変換後の推定された頂点のスクリーン座標
float oevminx, oevmaxx, oevminy, oevmaxy, oevdiffx, oevdiffy; // 透視変換の整合性のための変数
float tevminx, tevmaxx, tevminy, tevmaxy, tevdiffx, tevdiffy; // 透視変換の整合性のための変数
float tevminx0, tevmaxx0, tevminy0, tevmaxy0, tevdiffx0, tevdiffy0; // 透視変換の整合性のための変数

vector<vector<vector<vector5D>>> foreground_objects;
vector<vector<vector<vector2D>>> transfered_foreground_objects;
//vector<vector<vector5D>> foreground_objects;
//vector<map<int, vector<int>>> foreground_objects;
vector<vector5D> current_selection;

map<int, vector<int>, greater<int>> current_contour;

ImageInfo ii;


int last_x,last_y;
int start_x, start_y;

/* OpenGL関係の変数 */
//unsigned char * global_data;
int global_width, global_height;
int main_window;
GLUI *glui;

int   wireframe = 0;
int   segments = 8;

GLuint loadTexture(const char * filename)
{
	GLuint texture = 0;

	cv::Mat tmp = cv::imread(filename);
	cv::Mat inputImage;
	cv::flip(tmp, inputImage, 0);

	ii.originalImage = inputImage.clone();
	ii.background = ii.originalImage.clone();

	ii.targetMask.create(ii.originalImage.size(), CV_8UC1);
	ii.targetMask.setTo(0);
    ii.sourceMask.create(ii.originalImage.size(), CV_8UC1);
	ii.sourceMask.setTo(0);

	global_width = ii.originalImage.cols;
	global_height = ii.originalImage.rows;

	return texture;
}

// 推定された頂点のスクリーン座標の取得
void getEstimatedVerticesScreenCoordinates(void)
{
	int i;

	for (i = 0; i<4; i++){
		gradient[i] = (ctrlPoint[i + 1].sy - ctrlPoint[0].sy) / (ctrlPoint[i + 1].sx - ctrlPoint[0].sx);
	}
	// 消失点（0）
	estimatedVertex[0].sx = ctrlPoint[0].sx;
	estimatedVertex[0].sy = ctrlPoint[0].sy;
	// インナーレクタングルの頂点（1, 2, 8, 7）
	// 左下（1）
	estimatedVertex[1].sx = ctrlPoint[1].sx;
	estimatedVertex[1].sy = ctrlPoint[1].sy;
	// 右下（2）
	estimatedVertex[2].sx = ctrlPoint[2].sx;
	estimatedVertex[2].sy = ctrlPoint[2].sy;
	// 右上（8）
	estimatedVertex[8].sx = ctrlPoint[3].sx;
	estimatedVertex[8].sy = ctrlPoint[3].sy;
	// 左上（7）
	estimatedVertex[7].sx = ctrlPoint[4].sx;
	estimatedVertex[7].sy = ctrlPoint[4].sy;
	// （3, 5）
	estimatedVertex[3].sx = -(ctrlPoint[0].sy + 1.0) / gradient[0] + ctrlPoint[0].sx;
	estimatedVertex[3].sy = -1.0; 
  	estimatedVertex[5].sx = -1.0;
	estimatedVertex[5].sy = (-1.0 - ctrlPoint[0].sx)*gradient[0] + ctrlPoint[0].sy;
	// （4, 6）
	estimatedVertex[4].sx = (-1.0f - ctrlPoint[0].sy) / gradient[1] + ctrlPoint[0].sx;
	estimatedVertex[4].sy = -1.0f; 
  	estimatedVertex[6].sx = 1.0f;
	estimatedVertex[6].sy = (1.0f - ctrlPoint[0].sx)*gradient[1] + ctrlPoint[0].sy;
	// （10, 12）
	estimatedVertex[10].sx = (1.0f - ctrlPoint[0].sy) / gradient[2] + ctrlPoint[0].sx;
	estimatedVertex[10].sy = 1.0f; 
  	estimatedVertex[12].sx = 1.0f;
	estimatedVertex[12].sy = (1.0f - ctrlPoint[0].sx)*gradient[2] + ctrlPoint[0].sy;
	// （9, 11）
	estimatedVertex[9].sx = (1.0f - ctrlPoint[0].sy) / gradient[3] + ctrlPoint[0].sx;
	estimatedVertex[9].sy = 1.0f; 
  	estimatedVertex[11].sx = -1.0f;
	estimatedVertex[11].sy = (-1.0f - ctrlPoint[0].sx)*gradient[3] + ctrlPoint[0].sy;
}

// 推定された頂点のワールド座標の取得
void getEstimatedVerticesWorldCoordinates()
{
	
	float grad; // 傾き
	float height; // 直方体の高さ

  // 視点のワールド座標の取得
  // （視点と消失点を結ぶ直線はスクリーン（z=-1.0f）に垂直である）
	eye.x = ctrlPoint[0].sx; eye.y = ctrlPoint[0].sy; eye.z = 0.0f;
	//printf("eye: %12f %12f %12f\n",eye.x,eye.y,eye.z);
	// floor上（y=0.0f）の推定された頂点（1, 2, 3, 4, 5, 6）のワールド座標の取得
	for (int i = 1; i <= 6; i++)
	{
		grad = -(1.0+eye.y) / (estimatedVertex[i].sy - eye.y);
    
		estimatedVertex[i].x = grad * (estimatedVertex[i].sx - eye.x) + eye.x;
		estimatedVertex[i].z = grad * (-1.0f - eye.z) + eye.z;
		estimatedVertex[i].y = -1.0f;
	}
	// 消失点（注視点）のワールド座標の取得
	estimatedVertex[0].x = ctrlPoint[0].sx;
	estimatedVertex[0].y = ctrlPoint[0].sy;
	estimatedVertex[0].z = estimatedVertex[1].z;
	
	// 視点と消失点（注視点）との距離
	R = R0 = -estimatedVertex[0].z;
	// 直方体の高さの取得
	height = (estimatedVertex[7].sy - estimatedVertex[1].sy) / (-1.0f)*estimatedVertex[1].z;
	
	// rear wall上の推定された頂点（7,8）のワールド座標の取得

	estimatedVertex[7].x = estimatedVertex[1].x; estimatedVertex[8].x = estimatedVertex[2].x;
	estimatedVertex[7].y = estimatedVertex[8].y = height -1.0;
	estimatedVertex[7].z = estimatedVertex[1].z; estimatedVertex[8].z = estimatedVertex[2].z;
	// ceiling上の推定された頂点（9, 10, 11, 12）のワールド座標の取得
	for (int i = 9; i <= 12; i++)
	{
		grad = (height - eye.y - 1.0) / (estimatedVertex[i].sy - eye.y);
		estimatedVertex[i].x = grad * (estimatedVertex[i].sx - eye.x) + eye.x;
		estimatedVertex[i].z = grad * (-1.0f - eye.z) + eye.z;
		estimatedVertex[i].y = height - 1.0;
	}
	
	//for foreground objs
	for(int i = 0; i < (int)foreground_objects.size(); i++){
		//vector<vector<vector5D>>::iterator iter = foreground_objects[i].begin();
		float ground = foreground_objects[i][0][0].sy;
		
		grad = -(1.0+eye.y) / (ground - eye.y);
		float z = grad * (-1.0f - eye.z) + eye.z;
		
		for(int j = 0; j < (int)foreground_objects[i].size(); j++){
			for(int k = 0; k < (int)foreground_objects[i][j].size(); k++){
			
				height = (foreground_objects[i][j][k].sy - ground) / (-1.0)*z;
				grad = (height - eye.y - 1.0) / (foreground_objects[i][j][k].sy - eye.y);
				foreground_objects[i][j][k].x = grad * (foreground_objects[i][j][k].sx - eye.x) + eye.x;
				foreground_objects[i][j][k].z = grad * (-1.0f - eye.z) + eye.z;
				foreground_objects[i][j][k].y = height - 1.0;
			}
			
		}
		
	}
	
	// 透視変換の整合性のための変数の取得
	oevminx = 0.0f, oevmaxx = 0.0f, oevminy = 0.0f, oevmaxy = 0.0f; //oevdiffx, oevdiffy;
	for(int i=1;i<=12;i++)
	{
		if(estimatedVertex[i].sx<oevminx) oevminx=estimatedVertex[i].sx;
		else if(estimatedVertex[i].sx>oevmaxx) oevmaxx=estimatedVertex[i].sx;
		if(estimatedVertex[i].sy<oevminy) oevminy=estimatedVertex[i].sy;
		else if(estimatedVertex[i].sy>oevmaxy) oevmaxy=estimatedVertex[i].sy;
	}
	oevdiffx = oevmaxx - oevminx; oevdiffy = oevmaxy - oevminy;
}



// 視点と透視変換行列の取得
void getPerspectiveTransferMatrix(void)
{
	float st, ct, sp, cp;

	st = (float)sin(theta); ct = (float)cos(theta);
	sp = (float)sin(phi); cp = (float)cos(phi);
	// 視点のワールド座標の取得
	eye.x = R * st*cp + estimatedVertex[0].x;
	eye.y = R * sp + estimatedVertex[0].y;
	eye.z = R * ct*cp + estimatedVertex[0].z;

	// 透視変換行列の取得
	mat[0] = ct; mat[1] = st * sp; mat[2] = -st * cp;
	mat[3] = 0.0f; mat[4] = cp; mat[5] = sp;
	mat[6] = st; mat[7] = -ct * sp; mat[8] = ct * cp;
}

// 透視変換後の推定された頂点のスクリーン座標の取得
void getTransferedEstimatedVerticesScreenCoordinates(void)
{
	int i;
	struct vector3D inte;
	struct vector3D diff;

	for (i = 1; i <= 12; i++)
	{
		diff.x = estimatedVertex[i].x - estimatedVertex[0].x;
		diff.y = estimatedVertex[i].y - estimatedVertex[0].y;
		diff.z = estimatedVertex[i].z - estimatedVertex[0].z;
		inte.x = diff.x*mat[0] + diff.y*mat[3] + diff.z*mat[6];
		inte.y = diff.x*mat[1] + diff.y*mat[4] + diff.z*mat[7];
		inte.z = diff.x*mat[2] + diff.y*mat[5] + diff.z*mat[8];
		transferedEstimatedVertex[i].sx = R * inte.x / (R - inte.z);
		transferedEstimatedVertex[i].sy = R * inte.y / (R - inte.z);
	}

	transferedEstimatedVertex[8].sy = 1.0;
	transferedEstimatedVertex[7].sy = 1.0;
	
	
	for(int i = 0; i < (int)foreground_objects.size(); i++){		
		for(int j = 0; j < (int)foreground_objects[i].size(); j++){
			for(int k = 0; k < (int)foreground_objects[i][j].size(); k++){
				diff.x = foreground_objects[i][j][k].x - estimatedVertex[0].x;
				diff.y = foreground_objects[i][j][k].y - estimatedVertex[0].y;
				diff.z = foreground_objects[i][j][k].z - estimatedVertex[0].z;
				inte.x = diff.x*mat[0] + diff.y*mat[3] + diff.z*mat[6];
				inte.y = diff.x*mat[1] + diff.y*mat[4] + diff.z*mat[7];
				inte.z = diff.x*mat[2] + diff.y*mat[5] + diff.z*mat[8];
				transfered_foreground_objects[i][j][k].sx = R * inte.x / (R - inte.z);
				transfered_foreground_objects[i][j][k].sy = R * inte.y / (R - inte.z);
				
			}
			
		}
		
	}
	
	// 透視変換の整合性のための変数の取得
	tevminx = 1.0f, tevmaxx = -1.0f, tevminy = 1.0f, tevmaxy = -1.0f; //tevdiffx, tevdiffy;
	for (i = 1; i <= 12; i++)
	{
		if(transferedEstimatedVertex[i].sx<tevminx) tevminx=transferedEstimatedVertex[i].sx;
		else if(transferedEstimatedVertex[i].sx>tevmaxx) tevmaxx=transferedEstimatedVertex[i].sx;
		if(transferedEstimatedVertex[i].sy<tevminy) tevminy=transferedEstimatedVertex[i].sy;
		else if(transferedEstimatedVertex[i].sy>tevmaxy) tevmaxy=transferedEstimatedVertex[i].sy;
	}
	tevdiffx = tevmaxx - tevminx; tevdiffy = tevmaxy - tevminy;
	if (deduceFlag != 1)
	{
		tevminx0 = 1.0f, tevmaxx0 = -1.0f, tevminy0 = 1.0f, tevmaxy0 = -1.0f; //tevdiffx0, tevdiffy0;
		for (i = 1; i <= 12; i++)
		{
			if(transferedEstimatedVertex[i].sx<tevminx0) tevminx0=transferedEstimatedVertex[i].sx;
			else if(transferedEstimatedVertex[i].sx>tevmaxx0) tevmaxx0=transferedEstimatedVertex[i].sx;
			if(transferedEstimatedVertex[i].sy<tevminy0) tevminy0=transferedEstimatedVertex[i].sy;
			else if(transferedEstimatedVertex[i].sy>tevmaxy0) tevmaxy0=transferedEstimatedVertex[i].sy;
		}
		tevdiffx0 = tevmaxx0 - tevminx0; tevdiffy0 = tevmaxy0 - tevminy0;
	}

	// 透視変換の整合
	for (i = 1; i <= 12; i++)
	{
		transferedEstimatedVertex[i].sx = (transferedEstimatedVertex[i].sx - tevminx) / tevdiffx;
		transferedEstimatedVertex[i].sy = (transferedEstimatedVertex[i].sy - tevminy) / tevdiffy;
		transferedEstimatedVertex[i].sx *= oevdiffx; transferedEstimatedVertex[i].sy *= oevdiffy;
		transferedEstimatedVertex[i].sx += oevminx; transferedEstimatedVertex[i].sy += oevminy;
	}
	
	for(int i = 0; i < (int)foreground_objects.size(); i++){		
		for(int j = 0; j < (int)foreground_objects[i].size(); j++){
			for(int k = 0; k < (int)foreground_objects[i][j].size(); k++){
				transfered_foreground_objects[i][j][k].sx = (transfered_foreground_objects[i][j][k].sx - tevminx) / tevdiffx;
				transfered_foreground_objects[i][j][k].sy = (transfered_foreground_objects[i][j][k].sy - tevminy) / tevdiffy;
				transfered_foreground_objects[i][j][k].sx *= oevdiffx; transfered_foreground_objects[i][j][k].sy *= oevdiffy;
				transfered_foreground_objects[i][j][k].sx += oevminx; transfered_foreground_objects[i][j][k].sy += oevminy;
			}
		}
	}

}

// スクリーン座標の取得
vector2D getScreenCoordinates(struct vector3D input)
{
	struct vector3D inte;
	struct vector3D diff;
	struct vector2D output;

	diff.x = input.x - estimatedVertex[0].x; diff.y = input.y - estimatedVertex[0].y; diff.z = input.z - estimatedVertex[0].z;
	inte.x = diff.x*mat[0] + diff.y*mat[3] + diff.z*mat[6];
	inte.y = diff.x*mat[1] + diff.y*mat[4] + diff.z*mat[7];
	inte.z = diff.x*mat[2] + diff.y*mat[5] + diff.z*mat[8];
	output.sx = R * inte.x / (R - inte.z); output.sy = R * inte.y / (R - inte.z);
	output.sx = (output.sx - tevminx) / tevdiffx; output.sy = (output.sy - tevminy) / tevdiffy;
	output.sx *= oevdiffx; output.sy *= oevdiffy; output.sx += oevminx; output.sy += oevminy;

	return output;
}

// 変形前のスクリーン座標の取得
vector2D getInitialScreenCoordinates(struct vector3D input)
{
	struct vector3D inte;
	struct vector3D diff;
	struct vector2D output;
	float st, ct, sp, cp;
	float inv[9];

	st = 0.0f; ct = 1.0f;
	sp = 0.0f; cp = 1.0f;
	// 視点のワールド座標の取得
	eye.x = R * st*cp + estimatedVertex[0].x;
	eye.y = R * sp + estimatedVertex[0].y;
	eye.z = R * ct*cp + estimatedVertex[0].z;
	
	// 透視変換行列の取得
	inv[0] = ct; inv[1] = st * sp; inv[2] = -st * cp;
	inv[3] = 0.0f; inv[4] = cp; inv[5] = sp;
	inv[6] = st; inv[7] = -ct * sp; inv[8] = ct * cp;

	diff.x = input.x - estimatedVertex[0].x; diff.y = input.y - estimatedVertex[0].y; diff.z = input.z - estimatedVertex[0].z;
	inte.x = diff.x*inv[0] + diff.y*inv[3] + diff.z*inv[6];
	inte.y = diff.x*inv[1] + diff.y*inv[4] + diff.z*inv[7];
	inte.z = diff.x*inv[2] + diff.y*inv[5] + diff.z*inv[8];
	output.sx = R * inte.x / (R - inte.z); output.sy = R * inte.y / (R - inte.z);
	output.sx = (output.sx - tevminx0) / tevdiffx0; output.sy = (output.sy - tevminy0) / tevdiffy0;
	output.sx *= oevdiffx; output.sy *= oevdiffy; output.sx += oevminx; output.sy += oevminy;

	return output;
}

// スパイダリーメッシュの描画
void drawSpideryMesh()
{
	glColor3f(0.0f, 0.2f, 1.0f);
	glLineWidth(3.0f);
	glBegin(GL_LINE_LOOP);
	// floor（1, 2, 4, 3）
	glVertex2f(estimatedVertex[1].sx, estimatedVertex[1].sy);
	glVertex2f(estimatedVertex[2].sx, estimatedVertex[2].sy);
	glVertex2f(estimatedVertex[4].sx, estimatedVertex[4].sy);
	glVertex2f(estimatedVertex[3].sx, estimatedVertex[3].sy);
	glEnd();
	glBegin(GL_LINE_LOOP);
	// ceiling (7, 9, 10, 8)
	glVertex2f(estimatedVertex[7].sx, estimatedVertex[7].sy);
	glVertex2f(estimatedVertex[9].sx, estimatedVertex[9].sy);
	glVertex2f(estimatedVertex[10].sx, estimatedVertex[10].sy);
	glVertex2f(estimatedVertex[8].sx, estimatedVertex[8].sy);
	glEnd();
	glBegin(GL_LINE_LOOP);
	// left wall（1, 7, 11, 5）
	glVertex2f(estimatedVertex[1].sx, estimatedVertex[1].sy);
	glVertex2f(estimatedVertex[7].sx, estimatedVertex[7].sy);
	glVertex2f(estimatedVertex[11].sx, estimatedVertex[11].sy);
	glVertex2f(estimatedVertex[5].sx, estimatedVertex[5].sy);
	glEnd();
	glBegin(GL_LINE_LOOP);
	// right wall（2, 8, 12, 6）
	glVertex2f(estimatedVertex[2].sx, estimatedVertex[2].sy);
	glVertex2f(estimatedVertex[8].sx, estimatedVertex[8].sy);
	glVertex2f(estimatedVertex[12].sx, estimatedVertex[12].sy);
	glVertex2f(estimatedVertex[6].sx, estimatedVertex[6].sy);
	glEnd();
	glLineWidth(2.5f);
	glBegin(GL_LINES);
	// rear wall (0, 1, 0, 2, 0, 8, 0, 7)
	glVertex2f(estimatedVertex[0].sx, estimatedVertex[0].sy);
	glVertex2f(estimatedVertex[1].sx, estimatedVertex[1].sy);
	glVertex2f(estimatedVertex[0].sx, estimatedVertex[0].sy);
	glVertex2f(estimatedVertex[2].sx, estimatedVertex[2].sy);
	glVertex2f(estimatedVertex[0].sx, estimatedVertex[0].sy);
	glVertex2f(estimatedVertex[8].sx, estimatedVertex[8].sy);
	glVertex2f(estimatedVertex[0].sx, estimatedVertex[0].sy);
	glVertex2f(estimatedVertex[7].sx, estimatedVertex[7].sy);
	glEnd();
	glColor3f(0.0f, 1.0f, 0.2f);
	glPointSize(3.0f);
	glBegin(GL_POINTS);
  
	// control points（0, 1, 2, 7, 8）
	glVertex2f(estimatedVertex[0].sx, estimatedVertex[0].sy);
	glVertex2f(estimatedVertex[1].sx, estimatedVertex[1].sy);
	glVertex2f(estimatedVertex[2].sx, estimatedVertex[2].sy);
	glVertex2f(estimatedVertex[7].sx, estimatedVertex[7].sy);
	glVertex2f(estimatedVertex[8].sx, estimatedVertex[8].sy);
	glEnd();
  glColor3f(1.0,1.0,1.0);

}

// 3次元背景の描画
void draw3Dbackground(void)
{
	float k;
	struct vector3D begin, end;
	struct vector2D bpoint, epoint;
	const float span = 0.1f;

	glColor3f(0.0f, 0.2f, 1.0f);
	/* boundary */
	glLineWidth(2.0f);
	glBegin(GL_LINE_LOOP);
	// floor（1, 2, 4, 3）
	glVertex2f(transferedEstimatedVertex[1].sx, transferedEstimatedVertex[1].sy);
	glVertex2f(transferedEstimatedVertex[2].sx, transferedEstimatedVertex[2].sy);
	glVertex2f(transferedEstimatedVertex[4].sx, transferedEstimatedVertex[4].sy);
	glVertex2f(transferedEstimatedVertex[3].sx, transferedEstimatedVertex[3].sy);
	glEnd();
	glBegin(GL_LINE_LOOP);
	// ceiling (7, 9, 10, 8)
	glVertex2f(transferedEstimatedVertex[7].sx, transferedEstimatedVertex[7].sy);
	glVertex2f(transferedEstimatedVertex[9].sx, transferedEstimatedVertex[9].sy);
	glVertex2f(transferedEstimatedVertex[10].sx, transferedEstimatedVertex[10].sy);
	glVertex2f(transferedEstimatedVertex[8].sx, transferedEstimatedVertex[8].sy);
	glEnd();
	glBegin(GL_LINE_LOOP);
	// left wall（1, 7, 11, 5）
	glVertex2f(transferedEstimatedVertex[1].sx, transferedEstimatedVertex[1].sy);
	glVertex2f(transferedEstimatedVertex[7].sx, transferedEstimatedVertex[7].sy);
	glVertex2f(transferedEstimatedVertex[11].sx, transferedEstimatedVertex[11].sy);
	glVertex2f(transferedEstimatedVertex[5].sx, transferedEstimatedVertex[5].sy);
	glEnd();
	glBegin(GL_LINE_LOOP);
	// right wall（2, 8, 12, 6）
	glVertex2f(transferedEstimatedVertex[2].sx, transferedEstimatedVertex[2].sy);
	glVertex2f(transferedEstimatedVertex[8].sx, transferedEstimatedVertex[8].sy);
	glVertex2f(transferedEstimatedVertex[12].sx, transferedEstimatedVertex[12].sy);
	glVertex2f(transferedEstimatedVertex[6].sx, transferedEstimatedVertex[6].sy);
	glEnd();
	/* mesh */
	glLineWidth(1.0f);
	glBegin(GL_LINES);
	// floor横線（z: 1～3）
	begin.x = estimatedVertex[1].x; end.x = estimatedVertex[2].x;
	begin.y = end.y = estimatedVertex[1].y;
	for (k = estimatedVertex[1].z; k <= estimatedVertex[3].z; k += span)
	{
		begin.z = end.z = k;
		bpoint = getScreenCoordinates(begin); epoint = getScreenCoordinates(end);
		glVertex2f(bpoint.sx, bpoint.sy);
		glVertex2f(epoint.sx, epoint.sy);
	}
	// floor縦線（x: 1～2）
	begin.y = end.y = estimatedVertex[1].y;
	begin.z = estimatedVertex[1].z; end.z = estimatedVertex[3].z;
	for (k = estimatedVertex[1].x; k <= estimatedVertex[2].x; k += span)
	{
		begin.x = end.x = k;
		bpoint = getScreenCoordinates(begin); epoint = getScreenCoordinates(end);
		glVertex2f(bpoint.sx, bpoint.sy);
		glVertex2f(epoint.sx, epoint.sy);
	}
	// ceiling横線（z: 7～9）
	begin.x = estimatedVertex[7].x; end.x = estimatedVertex[8].x;
	begin.y = end.y = estimatedVertex[7].y;
	for (k = estimatedVertex[7].z; k <= estimatedVertex[9].z; k += span)
	{
		begin.z = end.z = k;
		bpoint = getScreenCoordinates(begin); epoint = getScreenCoordinates(end);
		glVertex2f(bpoint.sx, bpoint.sy);
		glVertex2f(epoint.sx, epoint.sy);
	}
	// ceiling縦線（x: 7～8）
	begin.y = end.y = estimatedVertex[7].y;
	begin.z = estimatedVertex[7].z; end.z = estimatedVertex[9].z;
	for (k = estimatedVertex[7].x; k <= estimatedVertex[8].x; k += span)
	{
		begin.x = end.x = k;
		bpoint = getScreenCoordinates(begin); epoint = getScreenCoordinates(end);
		glVertex2f(bpoint.sx, bpoint.sy);
		glVertex2f(epoint.sx, epoint.sy);
	}
	// left wall横線（y: 1～7）
	begin.x = end.x = estimatedVertex[1].x;
	begin.z = estimatedVertex[1].z; end.z = estimatedVertex[5].z;
	for (k = estimatedVertex[1].y; k <= estimatedVertex[7].y; k += span)
	{
		begin.y = end.y = k;
		bpoint = getScreenCoordinates(begin); epoint = getScreenCoordinates(end);
		glVertex2f(bpoint.sx, bpoint.sy);
		glVertex2f(epoint.sx, epoint.sy);
	}
	glEnd();
	// left wall縦線（z: 1～5）
	begin.x = end.x = estimatedVertex[1].x;
	begin.y = estimatedVertex[1].y; end.y = estimatedVertex[7].y;
	glBegin(GL_LINES);
	for (k = estimatedVertex[1].z; k <= estimatedVertex[5].z; k += span)
	{
		begin.z = end.z = k;
		bpoint = getScreenCoordinates(begin); epoint = getScreenCoordinates(end);
		glVertex2f(bpoint.sx, bpoint.sy);
		glVertex2f(epoint.sx, epoint.sy);
	}
	// right wall横線（y: 2～8）
	begin.x = end.x = estimatedVertex[2].x;
	begin.z = estimatedVertex[2].z; end.z = estimatedVertex[6].z;
	for (k = estimatedVertex[2].y; k <= estimatedVertex[8].y; k += span)
	{
		begin.y = end.y = k;
		bpoint = getScreenCoordinates(begin); epoint = getScreenCoordinates(end);
		glVertex2f(bpoint.sx, bpoint.sy);
		glVertex2f(epoint.sx, epoint.sy);
	}
	// right wall縦線（z: 2～6）
	begin.x = end.x = estimatedVertex[2].x;
	begin.y = estimatedVertex[2].y; end.y = estimatedVertex[8].y;
	for (k = estimatedVertex[2].z; k <= estimatedVertex[6].z; k += span)
	{
		begin.z = end.z = k;
		bpoint = getScreenCoordinates(begin); epoint = getScreenCoordinates(end);
		glVertex2f(bpoint.sx, bpoint.sy);
		glVertex2f(epoint.sx, epoint.sy);
	}
	// rear wall横線（y: 1～7）
	begin.x = estimatedVertex[1].x; end.x = estimatedVertex[2].x;
	begin.z = end.z = estimatedVertex[1].z;
	for (k = estimatedVertex[1].y; k <= estimatedVertex[7].y; k += span)
	{
		begin.y = end.y = k;
		bpoint = getScreenCoordinates(begin); epoint = getScreenCoordinates(end);
		glVertex2f(bpoint.sx, bpoint.sy);
		glVertex2f(epoint.sx, epoint.sy);
	}
	// rear wall縦線（x: 1～2）
	begin.y = estimatedVertex[1].y; end.y = estimatedVertex[7].y;
	begin.z = end.z = estimatedVertex[1].z;
	for (k = estimatedVertex[1].x; k <= estimatedVertex[2].x; k += span)
	{
		begin.x = end.x = k;
		bpoint = getScreenCoordinates(begin); epoint = getScreenCoordinates(end);
		glVertex2f(bpoint.sx, bpoint.sy);
		glVertex2f(epoint.sx, epoint.sy);
	}
	glEnd();
  glColor3f(1.0,1.0,1.0);
}

// 元画像の引数のスクリーン座標の色を返す関数
color3D screenCoordinates2sourceImageColor(vector2D input, bool is_bg)
{
	color3D output;
	int x, y; // 画像スケールでのx, y座標
	//int headindex; // x, y座標のピクセルの色情報の先頭（赤）のインデックス

	if (input.sx<-1.0f) output.r = output.g = output.b = 1.0f;
	else if (input.sx>1.0f) output.r = output.g = output.b = 1.0f;
	else if (input.sy<-1.0f) output.r = output.g = output.b = 1.0f;
	else if (input.sy>1.0f) output.r = output.g = output.b = 1.0f;
	else
	{
		// 画像スケールでのx, y座標の取得
    
		x = (int)((input.sx+1)*global_width/2.0); y = (int)((input.sy+1)*global_height/2.0);
		// x, y座標のピクセルの色情報の先頭（赤）のインデックスの取得
		// 色の取

		cv::Vec3b intensity;
		if(is_bg)
			intensity = ii.background.at<cv::Vec3b>(y, x);
		else
			intensity = ii.originalImage.at<cv::Vec3b>(y, x);
		output.b = intensity.val[0] / 255.0;
		output.g = intensity.val[1] / 255.0;
		output.r = intensity.val[2] / 255.0;
		
	}

	return output;
}

// 生成画像の描画
void drawGeneratedImage()
{
	int i;
	float k,l;
	struct vector3D worldVertex[4];
	struct vector2D screenVertex[4];
	struct vector3D worldGravityPoint;
	struct vector2D screenGravityPoint;
	struct color3D colorGravityPoint;

    glDisable(GL_TEXTURE_2D);
	glPolygonMode(GL_FRONT,GL_FILL); // CCWに塗りつぶし
	glCullFace(GL_BACK);
	glEnable(GL_CULL_FACE);	
    glBegin(GL_QUADS);

	// floor（z: 1～3, x: 1～2）
	worldVertex[0].y=worldVertex[1].y=worldVertex[2].y=worldVertex[3].y=estimatedVertex[1].y;
	for(k=estimatedVertex[1].z;k<estimatedVertex[3].z;k+=rough_coefficient)
	{
		worldVertex[0].z=worldVertex[3].z=k;
		worldVertex[1].z=worldVertex[2].z=k+rough_coefficient;
		for(l=estimatedVertex[1].x;l<estimatedVertex[2].x;l+=rough_coefficient)
		{
			worldVertex[0].x=worldVertex[1].x=l;
			worldVertex[2].x=worldVertex[3].x=l+rough_coefficient;
			// 共通処理
			// メッシュの各頂点の変形後のスクリーン座標の取得
			for(i=0;i<4;i++) screenVertex[i]=getScreenCoordinates(worldVertex[i]);
			// メッシュの重心のワールド座標の取得
			worldGravityPoint.x=(worldVertex[0].x+worldVertex[2].x)/2.0f;
			worldGravityPoint.y=(worldVertex[0].y+worldVertex[2].y)/2.0f;
			worldGravityPoint.z=(worldVertex[0].z+worldVertex[2].z)/2.0f;
			// メッシュの重心の変形前のスクリーン座標の取得
			screenGravityPoint=getInitialScreenCoordinates(worldGravityPoint);
			// メッシュの重心の変形前のスクリーン座標における元画像の色の取得
			colorGravityPoint=screenCoordinates2sourceImageColor(screenGravityPoint, true);
			glColor3f(colorGravityPoint.r,colorGravityPoint.g,colorGravityPoint.b);
			// メッシュを重心の変形前のスクリーン座標における元画像の色で塗りつぶし
			for(i=0;i<4;i++)glVertex2f(screenVertex[i].sx,screenVertex[i].sy);
		}
	}
	// ceiling（z: 7～9, x: 7～8） 
	worldVertex[0].y=worldVertex[1].y=worldVertex[2].y=worldVertex[3].y=estimatedVertex[7].y;
	for(k=estimatedVertex[7].z;k<estimatedVertex[9].z;k+=rough_coefficient)
	{
		worldVertex[0].z=worldVertex[1].z=k;
		worldVertex[2].z=worldVertex[3].z=k+rough_coefficient;
		for(l=estimatedVertex[7].x;l<estimatedVertex[8].x;l+=rough_coefficient)
		{
			worldVertex[0].x=worldVertex[3].x=l;
			worldVertex[1].x=worldVertex[2].x=l+rough_coefficient;
			// 共通処理
			// メッシュの各頂点の変形後のスクリーン座標の取得
			for(i=0;i<4;i++) screenVertex[i]=getScreenCoordinates(worldVertex[i]);
			// メッシュの重心のワールド座標の取得
			worldGravityPoint.x=(worldVertex[0].x+worldVertex[2].x)/2.0f;
			worldGravityPoint.y=(worldVertex[0].y+worldVertex[2].y)/2.0f;
			worldGravityPoint.z=(worldVertex[0].z+worldVertex[2].z)/2.0f;
			// メッシュの重心の変形前のスクリーン座標の取得
			screenGravityPoint=getInitialScreenCoordinates(worldGravityPoint);
			// メッシュの重心の変形前のスクリーン座標における元画像の色の取得
			colorGravityPoint=screenCoordinates2sourceImageColor(screenGravityPoint, true);
      glColor3f(colorGravityPoint.r,colorGravityPoint.g,colorGravityPoint.b);
			// メッシュを重心の変形前のスクリーン座標における元画像の色で塗りつぶし
			for(i=0;i<4;i++)glVertex2f(screenVertex[i].sx,screenVertex[i].sy);
		}
	}
	// left wall（y: 1～7, z: 1～5）
	worldVertex[0].x=worldVertex[1].x=worldVertex[2].x=worldVertex[3].x=estimatedVertex[1].x;
	for(k=estimatedVertex[1].y;k<estimatedVertex[7].y;k+=rough_coefficient)
	{
		worldVertex[0].y=worldVertex[3].y=k;
		worldVertex[1].y=worldVertex[2].y=k+rough_coefficient;
		for(l=estimatedVertex[1].z;l<estimatedVertex[5].z;l+=rough_coefficient)
		{
			worldVertex[0].z=worldVertex[1].z=l;
			worldVertex[2].z=worldVertex[3].z=l+rough_coefficient;
			// 共通処理
			// メッシュの各頂点の変形後のスクリーン座標の取得
			for(i=0;i<4;i++) screenVertex[i]=getScreenCoordinates(worldVertex[i]);
			// メッシュの重心のワールド座標の取得
			worldGravityPoint.x=(worldVertex[0].x+worldVertex[2].x)/2.0f;
			worldGravityPoint.y=(worldVertex[0].y+worldVertex[2].y)/2.0f;
			worldGravityPoint.z=(worldVertex[0].z+worldVertex[2].z)/2.0f;
			// メッシュの重心の変形前のスクリーン座標の取得
			screenGravityPoint=getInitialScreenCoordinates(worldGravityPoint);
			// メッシュの重心の変形前のスクリーン座標における元画像の色の取得
			colorGravityPoint=screenCoordinates2sourceImageColor(screenGravityPoint, true);
      glColor3f(colorGravityPoint.r,colorGravityPoint.g,colorGravityPoint.b);
			// メッシュを重心の変形前のスクリーン座標における元画像の色で塗りつぶし
			for(i=0;i<4;i++)glVertex2f(screenVertex[i].sx,screenVertex[i].sy);
		}
	}
	// right wall（y: 2～8, z: 2～6）
	worldVertex[0].x=worldVertex[1].x=worldVertex[2].x=worldVertex[3].x=estimatedVertex[2].x;
	for(k=estimatedVertex[2].y;k<estimatedVertex[8].y;k+=rough_coefficient)
	{
		worldVertex[0].y=worldVertex[1].y=k;
		worldVertex[2].y=worldVertex[3].y=k+rough_coefficient;
		for(l=estimatedVertex[2].z;l<estimatedVertex[6].z;l+=rough_coefficient)
		{
			worldVertex[0].z=worldVertex[3].z=l;
			worldVertex[1].z=worldVertex[2].z=l+rough_coefficient;
			// 共通処理
			// メッシュの各頂点の変形後のスクリーン座標の取得
			for(i=0;i<4;i++) screenVertex[i]=getScreenCoordinates(worldVertex[i]);
			// メッシュの重心のワールド座標の取得
			worldGravityPoint.x=(worldVertex[0].x+worldVertex[2].x)/2.0f;
			worldGravityPoint.y=(worldVertex[0].y+worldVertex[2].y)/2.0f;
			worldGravityPoint.z=(worldVertex[0].z+worldVertex[2].z)/2.0f;
			// メッシュの重心の変形前のスクリーン座標の取得
			screenGravityPoint=getInitialScreenCoordinates(worldGravityPoint);
			// メッシュの重心の変形前のスクリーン座標における元画像の色の取得
			colorGravityPoint=screenCoordinates2sourceImageColor(screenGravityPoint, true);
      glColor3f(colorGravityPoint.r,colorGravityPoint.g,colorGravityPoint.b);
			// メッシュを重心の変形前のスクリーン座標における元画像の色で塗りつぶし
			for(i=0;i<4;i++)glVertex2f(screenVertex[i].sx,screenVertex[i].sy);
		}
	}
	// rear wall（y: 1～7, x: 1～2）
	worldVertex[0].z=worldVertex[1].z=worldVertex[2].z=worldVertex[3].z=estimatedVertex[1].z;
	for(k=estimatedVertex[1].y;k<estimatedVertex[7].y;k+=rough_coefficient)
	{
		worldVertex[0].y=worldVertex[1].y=k;
		worldVertex[2].y=worldVertex[3].y=k+rough_coefficient;
		for(l=estimatedVertex[1].x;l<estimatedVertex[2].x;l+=rough_coefficient)
		{
			worldVertex[0].x=worldVertex[3].x=l;
			worldVertex[1].x=worldVertex[2].x=l+rough_coefficient;
			// 共通処理
			// メッシュの各頂点の変形後のスクリーン座標の取得
			for(i=0;i<4;i++) screenVertex[i]=getScreenCoordinates(worldVertex[i]);
			// メッシュの重心のワールド座標の取得
			worldGravityPoint.x=(worldVertex[0].x+worldVertex[2].x)/2.0f;
			worldGravityPoint.y=(worldVertex[0].y+worldVertex[2].y)/2.0f;
			worldGravityPoint.z=(worldVertex[0].z+worldVertex[2].z)/2.0f;
			// メッシュの重心の変形前のスクリーン座標の取得
			screenGravityPoint=getInitialScreenCoordinates(worldGravityPoint);
			// メッシュの重心の変形前のスクリーン座標における元画像の色の取得
			colorGravityPoint=screenCoordinates2sourceImageColor(screenGravityPoint, true);
			glColor3f(colorGravityPoint.r,colorGravityPoint.g,colorGravityPoint.b);
			// メッシュを重心の変形前のスクリーン座標における元画像の色で塗りつぶし
			for(i=0;i<4;i++)glVertex2f(screenVertex[i].sx,screenVertex[i].sy);
		}
	}
	
	/////////foreground obj
	
	for(int m = 0; m < (int)foreground_objects.size(); m++){
	
		worldVertex[0].z=worldVertex[1].z=worldVertex[2].z=worldVertex[3].z=foreground_objects[m][0][0].z;
		for(int n = 0; n < (int)foreground_objects[m].size() - 1; n++){
			for(k=foreground_objects[m][n][0].y;k<foreground_objects[m][n+1][0].y;k+=rough_coefficient)
			{
				worldVertex[0].y=worldVertex[1].y=k;
				worldVertex[2].y=worldVertex[3].y=k+rough_coefficient;
				for(int q = 0; q < (int)foreground_objects[m][n].size(); q += 2){
					for(l=foreground_objects[m][n][q].x;l<foreground_objects[m][n][q+1].x;l+=rough_coefficient)
					{
						worldVertex[0].x=worldVertex[3].x=l;
						worldVertex[1].x=worldVertex[2].x=l+rough_coefficient;
						// 共通処理
						// メッシュの各頂点の変形後のスクリーン座標の取得
						for(i=0;i<4;i++) screenVertex[i]=getScreenCoordinates(worldVertex[i]);
						// メッシュの重心のワールド座標の取得
						worldGravityPoint.x=(worldVertex[0].x+worldVertex[2].x)/2.0f;
						worldGravityPoint.y=(worldVertex[0].y+worldVertex[2].y)/2.0f;
						worldGravityPoint.z=(worldVertex[0].z+worldVertex[2].z)/2.0f;
						// メッシュの重心の変形前のスクリーン座標の取得
						screenGravityPoint=getInitialScreenCoordinates(worldGravityPoint);
						// メッシュの重心の変形前のスクリーン座標における元画像の色の取得
						colorGravityPoint=screenCoordinates2sourceImageColor(screenGravityPoint, false);
						glColor3f(colorGravityPoint.r,colorGravityPoint.g,colorGravityPoint.b);
						// メッシュを重心の変形前のスクリーン座標における元画像の色で塗りつぶし
						for(i=0;i<4;i++)glVertex2f(screenVertex[i].sx,screenVertex[i].sy);
					}
				}
			}
		}
	}
	
	
	glEnd();
	glDisable(GL_CULL_FACE);
  glColor3f(1.0,1.0,1.0);
}
/////////fore ground/////////////

void drawSelectedObj(){
	
	glColor3f(0.9,0.1,0.1);
	glLineWidth(0.8);
	
	//for(int i = 0; i < foreground_objects.size(); i++){
		glBegin(GL_LINES);
		int j = 0;
		int l = current_selection.size();
		for(; j < l-1; j++){
			glVertex2f(current_selection[j].sx, current_selection[j].sy);
			glVertex2f(current_selection[j+1].sx, current_selection[j+1].sy);
		}
		if(on_press == false && l > 0){
			glVertex2f(current_selection[j].sx, current_selection[j].sy);
			glVertex2f(current_selection[0].sx, current_selection[0].sy);
		}
		glEnd();
	//}
	glColor3f(1.0,1.0,1.0);
	
}

void drawTest(){
	map<int,vector<int>, greater<int>>::iterator iter = current_contour.begin();
	glColor3f(0.9,0.1,0.1);
	glLineWidth(0.8);
	float midx = global_width/2.0;
	float midy = global_height/2.0;
	glBegin(GL_LINES);
	while(iter != current_contour.end()){
				
		if(current_contour[iter->first].size() > 1){
			if((current_contour[iter->first].size() & 1) != 0){
				cout << "ERROR" << endl;
				//break;
				iter++;
				continue;
			}
			for(int i = 0; i < (int)current_contour[iter->first].size(); i += 2){
						
				glVertex2f(current_contour[iter->first][i]/midx - 1.0, 1.0 - (iter->first)/ midy);
				glVertex2f(current_contour[iter->first][i+1]/ midx - 1.0, 1.0 - (iter->first)/ midy);
									
			}
						
		}
					
		iter++;
	}
	glEnd();
	glColor3f(1.0,1.0,1.0);
	test_flag = false;
	
}


/////////////////////////////////

void myDisplay()
{
	if (glutGetWindow() != main_window)glutSetWindow(main_window);
	// バッファクリア
	glClear(GL_COLOR_BUFFER_BIT);
	// 元画像の描

	if (deduceFlag == 0){
		glRasterPos2i(0, 0);
    
		glBegin(GL_QUADS);
		glTexCoord2f(0.0f, 0.0f); 
		glVertex2i(-1, -1);
      
		glTexCoord2f(1.0f, 0.0f); 
		glVertex2i(1, -1);
     
		glTexCoord2f(1.0f, 1.0f); 
		glVertex2i(1, 1);
   
		glTexCoord2f(0.0f, 1.0f); 
		glVertex2i(-1, 1);
		glEnd();
    
    }
	// スパイダリーメッシュの描画
	if (deduceFlag == 0 && modenumber == 0) drawSpideryMesh();
	// 生成画像の描画
	//cout << modenumber << endl;
	if(deduceFlag == 0 && modenumber == 1) drawSelectedObj();
	
	if (deduceFlag) drawGeneratedImage();
	//i 3次元背景の描画
	if (deduceFlag && resolutionnumber == 0) draw3Dbackground();
	// 画像の表示

	if(test_flag) drawTest();
  glutSwapBuffers();
}



void myIdle(void)
{
	if (glutGetWindow() != main_window)glutSetWindow(main_window);
	// 画像の表示
	glutSwapBuffers();
}

void myReshape(int x, int y)
{
	// ウィンドウサイズの変更
	glutReshapeWindow(global_width, global_height);
  
	glutPostRedisplay();
}

void myKeyboard(unsigned char key, int x, int y)
{
	switch (key)
	{
		//case 27:
		case 'q':
			exit(0);
			break;
		if (deduceFlag){
			// move forward
			case 'w':
				if(estimatedVertex[0].z < -1)
					estimatedVertex[0].z += 0.01;
				// 視点と透視変換行列の取得
				getPerspectiveTransferMatrix();
				// 透視変換後の推定された頂点のスクリーン座標の取得
				getTransferedEstimatedVerticesScreenCoordinates();
				//glutPostRedisplay();
				break;
			// move background
			case 's':
				estimatedVertex[0].z -= 0.01;
				// 視点と透視変換行列の取得
				getPerspectiveTransferMatrix();
				// 透視変換後の推定された頂点のスクリーン座標の取得
				getTransferedEstimatedVerticesScreenCoordinates();
				//glutPostRedisplay();
				break;
			case 't':
				test_flag= true;
				//for (int i = 0; i < 50000; i++)
				//	global_data[i] = 0;
				//glutPostRedisplay();
				//glTexSubImage2D(GL_TEXTURE_2D, 0, 0,0, 100, 100, GL_RGB, GL_UNSIGNED_BYTE, global_data);
				//glTexImage2D(GL_TEXTURE_2D, 0,GL_RGB, global_width, global_height, 0, GL_BGR, GL_UNSIGNED_BYTE, global_data);
				break;
		}
	}

	glutPostRedisplay();
}

void myMouse(int button, int state, int x, int y)
{
	int i;
	vector2D pointedCoordinate;

	float threshold = 5.0 / (float)global_width*5.0 / (float)global_height; // 近傍と判断する閾値
	float diffx, diffy, dist; // クリックした点の制御点との距離（の自乗）
  
	float midx = global_width/2.0;
	float midy = global_height/2.0;

	switch (button)
	{
	case GLUT_LEFT_BUTTON:
		if (state == GLUT_DOWN)
		{
			on_press = true;
			last_x = x;
			last_y = y;
			start_x = x;
			start_y = y;
			
			current_selection.clear();
			current_contour.clear();
			pointedCoordinate.sx = x/midx-1.0;
			pointedCoordinate.sy = -(y/midy-1.0);
			if (deduceFlag)
			{
				pbeginsx = pointedCoordinate.sx;
				pbeginsy = pointedCoordinate.sy;
			}
			else
			{
				for (i = 0; i<5; i++)
				{
					diffx = pointedCoordinate.sx - ctrlPoint[i].sx;
					diffy = pointedCoordinate.sy - ctrlPoint[i].sy;
					dist = diffx * diffx + diffy * diffy;
					if (dist <= threshold)
					{
						selectedCtrlPointIndex = i;
						threshold = dist;
					}
				}
			}
		}
		else if (state == GLUT_UP)
		{
			if(modenumber == 1){
				int cur_y = last_y;
				float grad = (start_x - last_x)*1.0 / (start_y - last_y);
				int dir = (start_y > last_y) ? 1 : -1;
				int cur_x = last_x;
				int count = 1;
				//cout << "aaaa" << endl;
				while(cur_y != start_y){
					//cout << "cccc" << endl;
					if(current_contour.count(cur_y) == 0)
						current_contour[cur_y] = vector<int>();
					current_contour[cur_y].push_back(cur_x);
					cur_y += dir;
					cur_x = round(last_x + grad*count*dir);
					count++;
				}
			}
			on_press = false;
			selectedCtrlPointIndex = -1;
			
		}
		//glutPostRedisplay();
		break;
	case GLUT_RIGHT_BUTTON:
		//cout << "press" << endl;
		if(modenumber == 1){

			current_selection.clear();
			current_contour.clear();

		}
		break;
	}
	glutPostRedisplay();
}

void initEstimatedVertices(){
  ctrlPoint[0].sx = 0.0;
  ctrlPoint[0].sy = 0.0;

  ctrlPoint[1].sx = -0.25;
  ctrlPoint[1].sy = -0.25;

  ctrlPoint[2].sx = 0.25;
  ctrlPoint[2].sy = -0.25;

  ctrlPoint[3].sx = 0.25;
  ctrlPoint[3].sy = 0.25;

  ctrlPoint[4].sx = -0.25;
  ctrlPoint[4].sy = 0.25;
  getEstimatedVerticesScreenCoordinates();

}

void myMotion(int x, int y)
{

	vector2D pointedCoordinate;
    
	float midx = global_width/2.0;
	float midy = global_height/2.0;
	pointedCoordinate.sx = (x/midx-1.0);
	pointedCoordinate.sy = -(y/midy-1.0);
  
	if (deduceFlag)
	{
		if(!on_press) return;
		theta += (pbeginsx - pointedCoordinate.sx)*10.0f*PI / 180.0f;
		phi += (pbeginsy - pointedCoordinate.sy)*10.0f*PI / 180.0f;
		pbeginsx = pointedCoordinate.sx;
		pbeginsy = pointedCoordinate.sy;
		// 視点と透視変換行列の取得
		getPerspectiveTransferMatrix();
		// 透視変換後の推定された頂点のスクリーン座標の取得
		getTransferedEstimatedVerticesScreenCoordinates();
		glutPostRedisplay();
	}
	else if(modenumber == 0)
	{
		ctrlPoint[selectedCtrlPointIndex].sx = pointedCoordinate.sx;
		ctrlPoint[selectedCtrlPointIndex].sy = pointedCoordinate.sy;
		switch (selectedCtrlPointIndex)
		{
		case 0: // 消失点
			if (ctrlPoint[0].sx <= ctrlPoint[1].sx) ctrlPoint[0].sx = ctrlPoint[1].sx + 1.0f / (float)global_width;
			else if (ctrlPoint[0].sx >= ctrlPoint[3].sx) ctrlPoint[0].sx = ctrlPoint[3].sx - 1.0f / (float)global_width;
			if (ctrlPoint[0].sy <= ctrlPoint[1].sy) ctrlPoint[0].sy = ctrlPoint[1].sy + 1.0f / (float)global_height;
			else if (ctrlPoint[0].sy >= ctrlPoint[3].sy) ctrlPoint[0].sy = ctrlPoint[3].sy - 1.0f / (float)global_height;
			break;
		case 1: // 左下
			if (ctrlPoint[1].sx <= -1.0f) ctrlPoint[1].sx = -1.0f + 1.0f / (float)global_width;
			else if (ctrlPoint[1].sx >= ctrlPoint[0].sx) ctrlPoint[1].sx = ctrlPoint[0].sx - 1.0f / (float)global_width;
			if (ctrlPoint[1].sy >= ctrlPoint[0].sy) ctrlPoint[1].sy = ctrlPoint[0].sy - 1.0f / (float)global_height;
			else if (ctrlPoint[1].sy <= -1.0f) ctrlPoint[1].sy = -1.0f + 1.0f / (float)global_height;
			ctrlPoint[4].sx = ctrlPoint[1].sx; ctrlPoint[2].sy = ctrlPoint[1].sy;
			break;
		case 2: // 右下
			if (ctrlPoint[2].sx <= ctrlPoint[0].sx) ctrlPoint[2].sx = ctrlPoint[0].sx + 1.0f / (float)global_width;
			else if (ctrlPoint[2].sx >= 1.0f) ctrlPoint[2].sx = 1.0f - 1.0f / (float)global_width;
			if (ctrlPoint[2].sy >= ctrlPoint[0].sy) ctrlPoint[2].sy = ctrlPoint[0].sy - 1.0f / (float)global_height;
			else if (ctrlPoint[2].sy <= -1.0f) ctrlPoint[2].sy = -1.0f + 1.0f / (float)global_height;
			ctrlPoint[3].sx = ctrlPoint[2].sx; ctrlPoint[1].sy = ctrlPoint[2].sy;
			break;
		case 3: // 右上
			if (ctrlPoint[3].sx <= ctrlPoint[0].sx) ctrlPoint[3].sx = ctrlPoint[0].sx + 1.0f / (float)global_width;
			else if (ctrlPoint[3].sx >= 1.0f) ctrlPoint[3].sx = 1.0f - 1.0f / (float)global_width;
			if (ctrlPoint[3].sy >= 1.0f) ctrlPoint[3].sy = 1.0f - 1.0f / (float)global_height;
			else if (ctrlPoint[3].sy <= ctrlPoint[0].sy) ctrlPoint[3].sy = ctrlPoint[0].sy + 1.0f / (float)global_height;
			ctrlPoint[2].sx = ctrlPoint[3].sx; ctrlPoint[4].sy = ctrlPoint[3].sy;
			break;
		case 4: // 左上
			if (ctrlPoint[4].sx <= -1.0f) ctrlPoint[1].sx = -1.0f + 1.0f / (float)global_width;
			else if (ctrlPoint[4].sx >= ctrlPoint[0].sx) ctrlPoint[4].sx = ctrlPoint[0].sx - 1.0f / (float)global_width;
			if (ctrlPoint[4].sy >= 1.0f) ctrlPoint[4].sy = 1.0f - 1.0f / (float)global_height;
			else if (ctrlPoint[4].sy <= ctrlPoint[0].sy) ctrlPoint[4].sy = ctrlPoint[0].sy + 1.0f / (float)global_height;
			ctrlPoint[1].sx = ctrlPoint[4].sx; ctrlPoint[3].sy = ctrlPoint[4].sy;
			break;
		};
		// 推定された頂点のスクリーン座標の取得
		getEstimatedVerticesScreenCoordinates();
		glutPostRedisplay();
	}
	//foreground mode
	else {
		if(!on_press) return ;
		if(current_selection.size() == 0 || last_x != x || last_y != y){
			
			
			vector5D tmp;
			tmp.sx = pointedCoordinate.sx;
			tmp.sy = pointedCoordinate.sy;
			current_selection.push_back(tmp);

			if(last_y != y){
				int cur_y = last_y;
				float grad = (x - last_x)*1.0 / (y - last_y);
				int dir = (y > last_y) ? 1 : -1;
				int cur_x = last_x;
				int count = 1;
				
				while(cur_y != y){
					
					if(current_contour.count(cur_y) == 0)
						current_contour[cur_y] = vector<int>();
					current_contour[cur_y].push_back(cur_x);
					cur_y += dir;
					cur_x = round(last_x + grad*count*dir);
					count++;
				}
;
			}

			last_x = x;
			last_y = y;
		}
		glutPostRedisplay();
		
	}
}

// gluiコールバックのためのユーザID
#define SOURCEIMAGE_ID	200
#define DEDUCE_ID		300
#define INITIALIZE_ID	400
#define RESOLUTION_ID	500
#define MODE_ID	        600
#define ADD_ID	        700

// myGLUIで使われる変数
//GLUI_Listbox *sourceImageListbox;
GLUI_Button *deduceButton;
GLUI_Listbox *resolutionListbox;
GLUI_Button *initializeButton;

GLUI_Button *confirmButton;
GLUI_Listbox *modeListbox;


// コールバック関数
void control_cb(int control)
{
	
	switch (control)
	{
	case SOURCEIMAGE_ID:
		char bmpfilepath[270]; // bmpファイルへのパス（bmpファイル名は長さ260の文字配列型）
		// bmpファイルへのパスの取得
		sprintf(bmpfilepath, "images/%s", bmpfilename[currentfilenumber]);
		// 元画像のデータの取得
		//sourceimage = getBmpdata(bmpfilepath);
		loadTexture(bmpfilepath);
		// 推定された頂点の初期化
		initEstimatedVertices();
		// ウィンドウサイズの変更
		glutSetWindow(main_window);
		glutReshapeWindow(global_width, global_height);
		// 2次元平行投影の変更
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		// OpenGLでは2次元平行投影として透視投影はソフトで計算
		gluOrtho2D(-1.0f, 1.0f, -1.0f, 1.0f);
		glViewport(0, 0, global_width, global_height);
		glutPostRedisplay();
		break;
	case DEDUCE_ID:
		// 推定された頂点のワールド座標の取得
		getEstimatedVerticesWorldCoordinates();
		// 視点と透視変換行列の取得
		getPerspectiveTransferMatrix();
		// 透視変換後の推定された頂点のスクリーン座標の取得
		getTransferedEstimatedVerticesScreenCoordinates();
		// 旗を立てる
		deduceFlag = 1;
    
		// main_windowに反映
		if (glutGetWindow() != main_window)glutSetWindow(main_window);
		glutPostRedisplay();
    
		// 画像選択と3次元背景の推定のためのGLUIの無効化
		//sourceImageListbox->disable();
		deduceButton->disable();
		// 解像度選択のためのGLUI有効化
		resolutionListbox->enable();
    
		break;
        
    case MODE_ID:
		//in bg
		if(modenumber == 0){
			
			confirmButton->disable();
			
		}
		
		else if(modenumber == 1){
			current_contour.clear();
			current_selection.clear();
			foreground_objects.clear();
			
			confirmButton->enable();
			
		}
		
    
		// main_windowに反映
		if (glutGetWindow() != main_window)glutSetWindow(main_window);
    
		glutPostRedisplay();
    
		// 画像選択と3次元背景の推定のためのGLUIの無効化
		//sourceImageListbox->disable();
		deduceButton->enable();
    
		break;    
    case ADD_ID:
        
        if(current_selection.size() != 0){
			
			
			vector<vector<vector5D>> obj;
			vector<vector<vector2D>> transfered_obj;
			map<int,vector<int>, greater<int>>::iterator iter = current_contour.begin();

			float midx = global_width/2.0;
			float midy = global_height/2.0;

			while(iter != current_contour.end()){
						
				if(current_contour[iter->first].size() > 1){
					if((current_contour[iter->first].size() & 1) != 0){
						cout << "ERROR" << endl;
						//break;
						iter++;
						continue;
					}
					
					for(int i = 0; i < (int)current_contour[iter->first].size(); i += 2){
						
						for (int j = current_contour[iter->first][i]; j <= current_contour[iter->first][i+1]; j++)
							ii.targetMask.at<uchar>(global_height-iter->first, j) = 255;
						
					}
					
					vector<vector5D> row;
					vector<vector2D> transfered_row;
					for(int i = 0; i < (int)current_contour[iter->first].size(); i ++){
						vector5D tmp;
						tmp.sx = current_contour[iter->first][i]/midx - 1.0;
						tmp.sy = 1.0 - (iter->first)/ midy;

						row.push_back(tmp);
						vector2D tmp1;
						transfered_row.push_back(tmp1);
					}
					obj.push_back(row);
					transfered_obj.push_back(transfered_row);
				}
							
				iter++;
			}
			

			Inpaint::CriminisiInpainter inpainter;
			inpainter.setSourceImage(ii.background);
            inpainter.setTargetMask(ii.targetMask);
            inpainter.setSourceMask(ii.sourceMask);
            //inpainter.setPatchSize(5);
            inpainter.initialize();
			cv::Mat image = ii.background.clone();
			while(inpainter.hasMoreSteps()) {
				inpainter.step();
                inpainter.image().copyTo(image);
                image.setTo(cv::Scalar(0,250,0), inpainter.targetRegion());
			} 
			
			ii.background = inpainter.image().clone();
			//ii.displayImage = ii.originalImage.clone();
            ii.targetMask = inpainter.targetRegion().clone();
		
			glTexImage2D(GL_TEXTURE_2D, 0,GL_RGB, global_width, global_height, 0, GL_BGR, GL_UNSIGNED_BYTE, ii.background.data);
			
			foreground_objects.push_back(obj);
			transfered_foreground_objects.push_back(transfered_obj);
			current_selection.clear();
			current_contour.clear();
			
		}
        
    
		// main_windowに反映
		if (glutGetWindow() != main_window)glutSetWindow(main_window);
    
		glutPostRedisplay();
    
		// 画像選択と3次元背景の推定のためのGLUIの無効化
		break;    
        
        
	case INITIALIZE_ID:
		// 角度の初期化
		theta = 0.0f; phi = 0.0f;
		// 旗を折る
		deduceFlag = 0;
		glEnable(GL_TEXTURE_2D);
		// main_windowに反映
		if (glutGetWindow() != main_window) glutSetWindow(main_window);
		glutPostRedisplay();
		// 画像選択, 3次元背景の推定のためのGLUIの有効化
		//sourceImageListbox->enable();
		deduceButton->enable();
		// 解像度選択のためのGLUIの無効化
		resolutionListbox->disable();
		
		foreground_objects.clear();
		transfered_foreground_objects.clear();
		current_selection.clear();
		current_contour.clear();
		
		ii.background = ii.originalImage.clone();
		
		// 解像度選択のためのGLUIの初期化
		resolutionnumber = 0; glui->sync_live(); rough_coefficient = 0.05f; // Preview
		break;
	case RESOLUTION_ID:
		// 初期化と解像度選択のためのGLUIの無効化
		initializeButton->disable();
		resolutionListbox->disable();
		if (resolutionnumber == 0) rough_coefficient = 0.05f; // Preview
		else if (resolutionnumber == 1) rough_coefficient = 0.01f; // Low
		else if (resolutionnumber == 2) rough_coefficient = 0.005f; // Normal
		else if (resolutionnumber == 3) rough_coefficient = 0.001f; // High
		//if (glutGetWindow() != main_window)glutSetWindow(main_window);
		// main_windowに計算中を表示
		if (glutGetWindow() != main_window) glutSetWindow(main_window);
		// バッファクリア
		glClear(GL_COLOR_BUFFER_BIT);
		glColor3ub(0, 0, 0);
		glRasterPos2f(0.05f, 0.05f);
		int i;
		char message[] = "NOW CALCULATING";
		for (i = 0; i <= 15; i++)
			glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, message[i]);
		glutSwapBuffers();
		// main_windowに反映
		glutPostRedisplay();
		// 初期化と解像度選択のためのGLUIの有効化
		initializeButton->enable();
		resolutionListbox->enable();
		break;
	}
}


// GLUIコード
void myGlui()
{
	// glui windowの作成
	glui = GLUI_Master.create_glui("Controller", 0, 100, 100);

    glui->add_column(false);
   
	modeListbox = glui->add_listbox("Mode", &modenumber, MODE_ID, control_cb);
	modeListbox->add_item(0, "Background Grid");
	modeListbox->add_item(1, "Foreground Object");
	
    //glui->add_column(false);
    confirmButton = glui->add_button("Add to Foreground", ADD_ID, control_cb);
    confirmButton->disable();
    
	glui->add_column(false);
	deduceButton = glui->add_button("Deduce", DEDUCE_ID, control_cb);
	// glui->add_column(false);
	// resolutionListboxの作成
	resolutionListbox = glui->add_listbox("Resolution", &resolutionnumber, RESOLUTION_ID, control_cb);
	resolutionListbox->add_item(0, "Preview");
	resolutionListbox->add_item(1, "Low");
	resolutionListbox->add_item(2, "Normal");
	resolutionListbox->add_item(3, "High");
	resolutionListbox->disable();
	glui->add_column(false);
	initializeButton = glui->add_button("Reset", INITIALIZE_ID, control_cb);
}

void myInit(){
 
  glClearColor(0.0, 0.0, 0.0, 1.0);
  glEnable(GL_TEXTURE_2D);
  glEnable(GL_DEPTH_TEST);

  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);

  glTexImage2D(GL_TEXTURE_2D, 0,GL_RGB, global_width, global_height, 0, GL_BGR, GL_UNSIGNED_BYTE, ii.originalImage.data);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  
  initEstimatedVertices();

  glutPostRedisplay();
  // 2次元平行投影の変更
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  // OpenGLでは2次元平行投影として透視投影はソフトで計算
  gluOrtho2D(-1.0f, 1.0f, -1.0f, 1.0f);
  
}

int main(int argc, char **argv)
{

  texture = loadTexture( "./images/im5.jpg" );


	glutInit(&argc, argv);

	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
  glutInitWindowPosition(100, 200);
  glutInitWindowSize(global_width,global_height);
	
  main_window = glutCreateWindow("Tour Into the Picture");

	glutDisplayFunc(myDisplay);
	glutReshapeFunc(myReshape);
	glutKeyboardFunc(myKeyboard);
	glutMouseFunc(myMouse);
	glutMotionFunc(myMotion);
  
	myGlui();
	myInit();
  
  glui->set_main_gfx_window( main_window);
  
  glutMainLoop();

	return 0;
}
