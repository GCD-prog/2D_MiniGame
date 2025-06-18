//=============================================================================
//
//  『ジャストジャンプ DX5』
//
//  作成環境: C/C++ , DirectX 5 , Visual C++ 6.0
//
//  ゲームの概要:
//    スペースキーでジャンプして、迫ってくる壁や落とし穴をひたすら避け続ける
//    全5ステージのシンプルな2Dアクションゲームです。
//
//=============================================================================

//-----------------------------------------------------------------------------
// ■ STEP 1: ヘッダーファイルの読み込み
//-----------------------------------------------------------------------------
#define WIN32_LEAN_AND_MEAN
#include <windows.h>      // Windowsプログラミングの基本機能
#include <windowsx.h>     // Windowsプログラミングの便利な機能
#include <ddraw.h>        // DirectXの描画機能 (DirectDraw)
#include <mmsystem.h>     // 時間を扱う機能(timeGetTime)
#include <stdio.h>        // 文字列を扱う機能(wsprintf)

//-----------------------------------------------------------------------------
// ■ STEP 2: ライブラリの指定
//-----------------------------------------------------------------------------
#pragma comment(lib, "ddraw.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "dxguid.lib")

//-----------------------------------------------------------------------------
// ■ STEP 3: ゲーム全体で使う設定値 (マクロ)
//-----------------------------------------------------------------------------
#define APP_NAME            "ジャストジャンプ DX5"
#define SCREEN_WIDTH        640
#define SCREEN_HEIGHT       480
#define MAX_STAGES          5
#define MAX_POPUPS          5
#define MAX_OBSTACLES       3
#define NUM_GROUND_SEGMENTS 10

//-----------------------------------------------------------------------------
// ■ STEP 4: ゲームの状態を管理するための名前定義 (enum)
//   (番号に分かりやすい名前を付けて、プログラムを読みやすくします)
//-----------------------------------------------------------------------------
// ゲーム全体の進行状態
enum GameState{
    STATE_TITLE,
    STATE_PLAYING,
    STATE_STAGE_CLEAR,
    STATE_GAME_CLEAR,
    STATE_GAMEOVER
};

// プレイヤー個人の状態
enum PlayerState{
    PSTATE_NORMAL,
    PSTATE_MISS,
    PSTATE_RESPAWNING
};

//-----------------------------------------------------------------------------
// ■ STEP 5: プログラム全体で使う変数と設計図 (グローバル変数・構造体)
//   (どこからでも使える変数や、データのまとまりを定義します)
//-----------------------------------------------------------------------------

// --- DirectX関連 ---
LPDIRECTDRAW         g_pDD        = NULL; // DirectDrawの本体
LPDIRECTDRAWSURFACE  g_pDDSPrimary = NULL; // 実際に画面に表示される描画領域
LPDIRECTDRAWSURFACE  g_pDDSBack    = NULL; // 絵を描くための裏紙 (バックバッファ)
LPDIRECTDRAWCLIPPER  g_pDDClipper = NULL; // ウィンドウの外に描画しないための道具
HWND                 g_hwnd       = NULL; // 作成したウィンドウの識別ID

// --- ゲームの状態 ---
GameState g_eGameState = STATE_TITLE;   // 現在のゲーム状態
BOOL      g_bSpaceKeyWasDown = FALSE; // スペースキーが前に押されていたかを記録するフラグ

// --- 設計図 (構造体) ---
struct StageData      { float scrollSpeed; int clearScore; BOOL hasPits; };
struct Player         { float x, y; float vy; BOOL onGround; PlayerState state; DWORD stateChangeTime; };
struct Obstacle       { float x; int height; BOOL active; BOOL scored; };
struct ScorePopup     { BOOL active; float x, y; DWORD startTime; };
struct GroundSegment  { float x; int width; BOOL isPit; };

// --- ゲームで使う変数 ---
Player          g_Player;
Obstacle        g_Obstacles[MAX_OBSTACLES];
ScorePopup      g_ScorePopups[MAX_POPUPS];
GroundSegment   g_GroundSegments[NUM_GROUND_SEGMENTS];
StageData       g_StageSettings[MAX_STAGES];
DWORD           g_dwLastFrameTime = 0;
DWORD           g_dwScore = 0;
DWORD           g_dwCurrentStageScore = 0;
DWORD           g_dwHighScore = 0;
int             g_nPlayerLives = 0;
int             g_nCurrentStage = 0;

//-----------------------------------------------------------------------------
// ■ STEP 6: ゲームの物理法則や難易度に関する設定値 (定数)
//-----------------------------------------------------------------------------
const float GRAVITY = 0.4f;
const float JUMP_POWER = -10.0f;
const int   PLAYER_SIZE = 20;
const int   GROUND_Y = 400;
const int   OBSTACLE_WIDTH = 30;

//-----------------------------------------------------------------------------
// ■ STEP 7: これから作る関数の名前リスト (プロトタイプ宣言)
//   (プログラムの下の方で定義する関数の名前を、先に教えておきます)
//-----------------------------------------------------------------------------
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
BOOL Game_Init(HWND hwnd);
void Game_Shutdown();
void Game_Main();
void Flip_To_Screen();
void Update_Title();
void Draw_Title();
void Update_Playing();
void Draw_Playing();
void Update_StageClear(); void Draw_StageClear();
void Update_GameClear();  void Draw_GameClear();
void Update_GameOver();
void Draw_GameOver();
void Draw_Rect(int x, int y, int w, int h, int r, int g, int b);
void Reset_Game();
void StartNextStage();


//=============================================================================
// ◆ WinMain関数 - すべてのWindowsプログラムはここから始まります
//=============================================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASSEX wc;
    MSG msg;

    // 1. ウィンドウの設計図を準備します
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WindowProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = APP_NAME;
    wc.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassEx(&wc)){
        return 0;
    }

    // 2. ウィンドウのスタイルとサイズを計算します
    DWORD dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

    RECT  wr = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };

    AdjustWindowRect(&wr, dwStyle, FALSE);

    // 3. ウィンドウを画面の中央に作成します
    g_hwnd = CreateWindow(APP_NAME, APP_NAME, dwStyle, (GetSystemMetrics(SM_CXSCREEN) - (wr.right - wr.left)) / 2, (GetSystemMetrics(SM_CYSCREEN) - (wr.bottom - wr.top)) / 2, wr.right - wr.left, wr.bottom - wr.top, NULL, NULL, hInstance, NULL);

    if (g_hwnd == NULL){
        return 0;
    }

    // 4. ウィンドウを表示します
    ShowWindow(g_hwnd, nCmdShow);
    UpdateWindow(g_hwnd);

    // 5. ゲームの初期化処理を呼び出します
    if (!Game_Init(g_hwnd)){
        DestroyWindow(g_hwnd);
        return 0;
    }

    // 6. ゲームのメインループです。この中をずっと回り続けます
    while (TRUE){
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)){
            if (msg.message == WM_QUIT){
                break; // ループを抜けます
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }else{

            // Windowsからのメッセージがなければ、ゲームの処理を1フレーム進めます
            Game_Main();
        }
    }

    // 7. ゲームの終了処理です
    Game_Shutdown();
    return (int)msg.wParam;
}

//=============================================================================
// ◆ WindowProc関数 - ウィンドウに関するイベント(メッセージ)を処理します
//=============================================================================
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg){
        case WM_DESTROY:
        {
            PostQuitMessage(0); // ゲーム終了のメッセージを送ります
            return 0;
        }
    }

    // 特に処理しないメッセージはWindowsに任せます
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

//=============================================================================
// ◆ Game_Init関数 - ゲームの初期化を行います
//=============================================================================
BOOL Game_Init(HWND hwnd)
{
    HRESULT hr;

    // DirectXの準備
    hr = DirectDrawCreate(NULL, &g_pDD, NULL); if (FAILED(hr)) { return FALSE; }
    hr = g_pDD->SetCooperativeLevel(hwnd, DDSCL_NORMAL); if (FAILED(hr)) { return FALSE; }

    // 描画領域(サーフェス)の準備
    DDSURFACEDESC ddsd;
    ZeroMemory(&ddsd, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    ddsd.dwFlags = DDSD_CAPS;
    ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
    hr = g_pDD->CreateSurface(&ddsd, &g_pDDSPrimary, NULL); if (FAILED(hr)) { return FALSE; }
    
    ddsd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
    ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
    ddsd.dwWidth = SCREEN_WIDTH;
    ddsd.dwHeight = SCREEN_HEIGHT;
    hr = g_pDD->CreateSurface(&ddsd, &g_pDDSBack, NULL); if (FAILED(hr)) { return FALSE; }
    
    // クリッパー(はみ出し防止)の準備
    hr = g_pDD->CreateClipper(0, &g_pDDClipper, NULL); if (FAILED(hr)) { return FALSE; }
    hr = g_pDDClipper->SetHWnd(0, hwnd); if (FAILED(hr)) { return FALSE; }
    hr = g_pDDSPrimary->SetClipper(g_pDDClipper); if (FAILED(hr)) { return FALSE; }

    // 全ステージの設定
	// ステージ1
    g_StageSettings[0].scrollSpeed = -4.0f; 
	g_StageSettings[0].clearScore = 150; 
	g_StageSettings[0].hasPits = FALSE;

	// ステージ2
    g_StageSettings[1].scrollSpeed = -4.5f; 
	g_StageSettings[1].clearScore = 150; 
	g_StageSettings[1].hasPits = TRUE;

	// ステージ3
    g_StageSettings[2].scrollSpeed = -5.0f; 
	g_StageSettings[2].clearScore = 150; 
	g_StageSettings[2].hasPits = TRUE;

	// ステージ4
    g_StageSettings[3].scrollSpeed = -5.5f; 
	g_StageSettings[3].clearScore = 150; 
	g_StageSettings[3].hasPits = TRUE;

	// ステージ5
    g_StageSettings[4].scrollSpeed = -6.0f; 
	g_StageSettings[4].clearScore = 150; 
	g_StageSettings[4].hasPits = TRUE;

    // ゲーム変数の初期化
    srand(timeGetTime());
    g_eGameState = STATE_TITLE;
    g_dwLastFrameTime = timeGetTime();

    return TRUE;
}

//=============================================================================
// ◆ Game_Shutdown関数 - ゲーム終了時に後片付けをします
//=============================================================================
void Game_Shutdown()
{
    if (g_pDDClipper)  { g_pDDClipper->Release();  g_pDDClipper = NULL;  }
    if (g_pDDSBack)    { g_pDDSBack->Release();    g_pDDSBack = NULL;    }
    if (g_pDDSPrimary) { g_pDDSPrimary->Release(); g_pDDSPrimary = NULL; }
    if (g_pDD)         { g_pDD->Release();         g_pDD = NULL;         }
}

//=============================================================================
// ◆ Game_Main関数 - ゲームループの本体。状態に応じて処理を振り分けます
//=============================================================================
void Game_Main()
{
    DWORD currentTime = timeGetTime();

    if (currentTime - g_dwLastFrameTime < 16){
        return;
    }

    g_dwLastFrameTime = currentTime;

    switch (g_eGameState){

        case STATE_TITLE:       
			Update_Title();
			Draw_Title();
			break;

        case STATE_PLAYING:
			Update_Playing();
			Draw_Playing();
			break;

        case STATE_STAGE_CLEAR:
			Update_StageClear();
			Draw_StageClear();
			break;

        case STATE_GAME_CLEAR:
			Update_GameClear();
			Draw_GameClear();
			break;

        case STATE_GAMEOVER:
			Update_GameOver();
			Draw_GameOver();
			break;

    }
}

//=============================================================================
// ◆ Flip_To_Screen関数 - 裏紙(バックバッファ)の内容を表画面に転送します
//=============================================================================
void Flip_To_Screen()
{
    RECT rcSrc, rcDest;
    POINT p = { 0, 0 };
    SetRect(&rcSrc, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    GetClientRect(g_hwnd, &rcDest);
    ClientToScreen(g_hwnd, &p);
    OffsetRect(&rcDest, p.x, p.y);
    g_pDDSPrimary->Blt(&rcDest, g_pDDSBack, &rcSrc, DDBLT_WAIT, NULL);
}

//=============================================================================
// ◆ タイトル画面の処理
//=============================================================================
void Update_Title()
{
    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) { 
		PostMessage(g_hwnd, WM_CLOSE, 0, 0); 
	}

    if (GetAsyncKeyState(VK_SPACE) & 0x8000) {
		g_bSpaceKeyWasDown = TRUE;
	}else{ 
		if (g_bSpaceKeyWasDown) { 
			g_eGameState = STATE_PLAYING; Reset_Game(); 
			g_bSpaceKeyWasDown = FALSE; 
		} 
	}
}

void Draw_Title()
{
    HDC hdc; char szBuffer[128];
    Draw_Rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0, 80);
    if (SUCCEEDED(g_pDDSBack->GetDC(&hdc))){
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 0));
        TextOut(hdc, SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT / 2 - 80, "ジャストジャンプ DX5", lstrlen("ジャストジャンプ DX5"));
        SetTextColor(hdc, RGB(255, 255, 255));
        TextOut(hdc, SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT / 2, "スペースキーで はじめる", lstrlen("スペースキーで はじめる"));
        TextOut(hdc, SCREEN_WIDTH / 2 - 80, SCREEN_HEIGHT / 2 + 30, "ESCキーで おわる", lstrlen("ESCキーで おわる"));
        wsprintf(szBuffer, "ハイスコア：%d", g_dwHighScore);
        TextOut(hdc, SCREEN_WIDTH / 2 - 80, SCREEN_HEIGHT / 2 + 80, szBuffer, lstrlen(szBuffer));
        g_pDDSBack->ReleaseDC(hdc);
    }
    Flip_To_Screen();
}

//=============================================================================
// ◆ ゲームプレイ中の処理 (★★★ ここからがメインの処理です ★★★)
//=============================================================================
void Update_Playing()
{
    int i, j;
    DWORD currentTime = timeGetTime();
    float currentSpeed = g_StageSettings[g_nCurrentStage].scrollSpeed;

    // --- ESCキーが押されたらゲームを終了 ---
    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
    {
        PostMessage(g_hwnd, WM_CLOSE, 0, 0);
    }
    
    // --- 「+10」スコア演出の更新 (表示時間を過ぎたら消す) ---
    for (i = 0; i < MAX_POPUPS; i++)
    {
        if (g_ScorePopups[i].active)
        {
            if (currentTime - g_ScorePopups[i].startTime > 1000)
            {
                g_ScorePopups[i].active = FALSE;
            }
            else
            {
                g_ScorePopups[i].y -= 0.5f;
            }
        }
    }
    
    // --- 壁(障害物)を左にスクロールさせる ---
    for (i = 0; i < MAX_OBSTACLES; i++)
    {
        if (g_Obstacles[i].active)
        {
            g_Obstacles[i].x += currentSpeed;
            if (g_Obstacles[i].x < -OBSTACLE_WIDTH)
            {
                g_Obstacles[i].active = FALSE;
            }
        }
    }
    
    // --- 地面をスクロールさせ、新しい地面と壁を生成する ---
    for (i = 0; i < NUM_GROUND_SEGMENTS; i++)
    {
        g_GroundSegments[i].x += currentSpeed;
        
        // 地面パーツが画面の左端に完全に消えたら
        if (g_GroundSegments[i].x + g_GroundSegments[i].width < 0)
        {
            // まず、今ある地面の中で一番右端にあるものを探す
            float maxX = -9999.0f;
            for (j = 0; j < NUM_GROUND_SEGMENTS; j++)
            {
                float rightEdge = g_GroundSegments[j].x + (float)g_GroundSegments[j].width;
                if (rightEdge > maxX)
                {
                    maxX = rightEdge;
                }
            }
            
            // 新しい地面を、その一番右端のさらに右に生成する
            g_GroundSegments[i].x = maxX;
            
            // ステージ設定に応じて落とし穴にするか決める
            if (g_StageSettings[g_nCurrentStage].hasPits && (rand() % 5 == 0))
            {
                g_GroundSegments[i].isPit = TRUE;
                g_GroundSegments[i].width = 60 + rand() % 40;
            }
            else
            {
                g_GroundSegments[i].isPit = FALSE;
                g_GroundSegments[i].width = 100 + rand() % 200;
                
                // 新しい地面が落とし穴でなければ、確率で壁を置く
                if (rand() % 3 == 0)
                {
                    for (j = 0; j < MAX_OBSTACLES; j++)
                    {
                        if (!g_Obstacles[j].active)
                        {
                            g_Obstacles[j].active = TRUE;
                            g_Obstacles[j].scored = FALSE;
                            g_Obstacles[j].height = 30 + rand() % 50;
                            int random_pos = rand() % (g_GroundSegments[i].width - OBSTACLE_WIDTH);
                            g_Obstacles[j].x = g_GroundSegments[i].x + (float)random_pos;
                            break;
                        }
                    }
                }
            }
        }
    }

    // --- プレイヤーの状態に応じた処理 ---
    switch (g_Player.state)
    {
        case PSTATE_NORMAL:
        case PSTATE_RESPAWNING:
        {
            // プレイヤーの操作と物理演算
            if ((GetAsyncKeyState(VK_SPACE) & 0x8000) && g_Player.onGround)
            {
                g_Player.vy = JUMP_POWER;
                g_Player.onGround = FALSE;
            }
            g_Player.vy += GRAVITY;
            g_Player.y += g_Player.vy;
            
            // 着地判定
            BOOL onSolidGround = FALSE;
            for(i = 0; i < NUM_GROUND_SEGMENTS; i++) { if (g_Player.x + PLAYER_SIZE > g_GroundSegments[i].x && g_Player.x < g_GroundSegments[i].x + g_GroundSegments[i].width) { if (!g_GroundSegments[i].isPit) { onSolidGround = TRUE; } break; } }
            if (onSolidGround && g_Player.y >= GROUND_Y - PLAYER_SIZE) { g_Player.y = GROUND_Y - PLAYER_SIZE; g_Player.vy = 0; g_Player.onGround = TRUE; } else { g_Player.onGround = FALSE; }

            // ミス判定 (無敵時間中は行わない)
            BOOL isMiss = FALSE;
            if (g_Player.state == PSTATE_NORMAL)
            {
                for (i = 0; i < MAX_OBSTACLES; i++) { if (g_Obstacles[i].active) { RECT playerRect = { (int)g_Player.x, (int)g_Player.y, (int)g_Player.x + PLAYER_SIZE, (int)g_Player.y + PLAYER_SIZE }; RECT obstacleRect = { (int)g_Obstacles[i].x, GROUND_Y - g_Obstacles[i].height, (int)g_Obstacles[i].x + OBSTACLE_WIDTH, GROUND_Y }; RECT dest; if (IntersectRect(&dest, &playerRect, &obstacleRect)) { isMiss = TRUE; break; } } }
                if (!onSolidGround && g_Player.y > GROUND_Y) isMiss = TRUE; // 落とし穴
            }
            if (isMiss) { g_nPlayerLives--; g_Player.state = PSTATE_MISS; g_Player.stateChangeTime = currentTime; }
            
            // スコア加算
            for (i = 0; i < MAX_OBSTACLES; i++) { if (g_Obstacles[i].active && !g_Obstacles[i].scored && g_Obstacles[i].x + OBSTACLE_WIDTH < g_Player.x) { g_dwScore += 10; g_dwCurrentStageScore += 10; g_Obstacles[i].scored = TRUE; for (j = 0; j < MAX_POPUPS; j++) { if (!g_ScorePopups[j].active) { g_ScorePopups[j].active = TRUE; g_ScorePopups[j].x = g_Player.x; g_ScorePopups[j].y = g_Player.y - 15; g_ScorePopups[j].startTime = timeGetTime(); break; } } } }

            // ステージクリア判定
            if (g_dwCurrentStageScore >= (DWORD)g_StageSettings[g_nCurrentStage].clearScore) { g_eGameState = STATE_STAGE_CLEAR; g_bSpaceKeyWasDown = TRUE; }
            
            // 無敵時間の終了
            if (g_Player.state == PSTATE_RESPAWNING && currentTime - g_Player.stateChangeTime > 2000) { g_Player.state = PSTATE_NORMAL; }
            break;
        }
        case PSTATE_MISS:
        {
            // ミスした後、1秒後に復活するかゲームオーバーになる
            if (currentTime - g_Player.stateChangeTime > 1000) { if (g_nPlayerLives > 0) { g_Player.state = PSTATE_RESPAWNING; g_Player.stateChangeTime = currentTime; g_Player.x = 100; g_Player.y = GROUND_Y - PLAYER_SIZE; g_Player.vy = 0; g_Player.onGround = TRUE; } else { if (g_dwScore > g_dwHighScore) { g_dwHighScore = g_dwScore; } g_eGameState = STATE_GAMEOVER; g_bSpaceKeyWasDown = TRUE; } }
            break;
        }
    }
}

void Draw_Playing()
{
    int i; HDC hdc; char szBuffer[256];
 
	DDBLTFX ddbltfx; ZeroMemory(&ddbltfx, sizeof(ddbltfx)); 
	ddbltfx.dwSize = sizeof(DDBLTFX); 
	ddbltfx.dwFillColor = RGB(0, 0, 100); 
	g_pDDSBack->Blt(NULL, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &ddbltfx);
 
	for (i = 0; i < NUM_GROUND_SEGMENTS; i++) { 
		if (!g_GroundSegments[i].isPit) Draw_Rect((int)g_GroundSegments[i].x, GROUND_Y, g_GroundSegments[i].width, SCREEN_HEIGHT - GROUND_Y, 139, 69, 19);
	}

    for (i = 0; i < MAX_OBSTACLES; i++) { 
		if (g_Obstacles[i].active) Draw_Rect((int)g_Obstacles[i].x, GROUND_Y - g_Obstacles[i].height, OBSTACLE_WIDTH, g_Obstacles[i].height, 0, 200, 0); 
	}
 
	if (g_Player.state == PSTATE_RESPAWNING) { 
		if ((timeGetTime() / 100) % 2 == 0) Draw_Rect((int)g_Player.x, (int)g_Player.y, PLAYER_SIZE, PLAYER_SIZE, 255, 255, 0); 
	}else if (g_Player.state != PSTATE_MISS) {
		Draw_Rect((int)g_Player.x, (int)g_Player.y, PLAYER_SIZE, PLAYER_SIZE, 255, 255, 0); 
	}

    if (SUCCEEDED(g_pDDSBack->GetDC(&hdc))) { 
		SetBkMode(hdc, TRANSPARENT); 
		SetTextColor(hdc, RGB(255, 255, 255)); 
		int remainingScore = g_StageSettings[g_nCurrentStage].clearScore - g_dwCurrentStageScore; 
		
		if (remainingScore < 0) { 
			remainingScore = 0; 
		}
		
		wsprintf(szBuffer, "ステージ %d  スコア：%d (クリアまであと %d)", g_nCurrentStage + 1, g_dwScore, remainingScore); 
		TextOut(hdc, 10, 35, szBuffer, lstrlen(szBuffer)); 
		wsprintf(szBuffer, "いのち：%d", g_nPlayerLives); 
		TextOut(hdc, 10, 10, szBuffer, lstrlen(szBuffer)); 
		SetTextColor(hdc, RGB(255, 255, 150)); 

		for (i = 0; i < MAX_POPUPS; i++) { 

			if (g_ScorePopups[i].active) { 
				TextOut(hdc, (int)g_ScorePopups[i].x, (int)g_ScorePopups[i].y, "+10", 3); 
			} 

		} 
		g_pDDSBack->ReleaseDC(hdc); 
	}

    Flip_To_Screen();
}

//=============================================================================
// ◆ 各種画面の処理
//=============================================================================
void Update_StageClear() 
{ 
	if (GetAsyncKeyState(VK_SPACE) & 0x8000) { 
		g_bSpaceKeyWasDown = TRUE;
	}else{
		if (g_bSpaceKeyWasDown) {
			StartNextStage();
			g_bSpaceKeyWasDown = FALSE;
		}
	}
}

void Draw_StageClear()
{
	HDC hdc;
	char szBuffer[128];
	Draw_Rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 80, 0);
	
	if (SUCCEEDED(g_pDDSBack->GetDC(&hdc))) { 
		SetBkMode(hdc, TRANSPARENT); 
		SetTextColor(hdc, RGB(255, 255, 0)); 
		wsprintf(szBuffer, "ステージ %d クリア！", g_nCurrentStage + 1); TextOut(hdc, SCREEN_WIDTH / 2 - 120, SCREEN_HEIGHT / 2 - 40, szBuffer, lstrlen(szBuffer)); 
		SetTextColor(hdc, RGB(255, 255, 255)); TextOut(hdc, SCREEN_WIDTH / 2 - 140, SCREEN_HEIGHT / 2, "スペースキーで つぎのステージへ", lstrlen("スペースキーで つぎのステージへ")); 
		g_pDDSBack->ReleaseDC(hdc); 
	}
	
	Flip_To_Screen(); 
}

void Update_GameClear() 
{
	if (GetAsyncKeyState(VK_SPACE) & 0x8000) {
		g_bSpaceKeyWasDown = TRUE;
	}else{ 
		if (g_bSpaceKeyWasDown) { 
			g_eGameState = STATE_TITLE; g_bSpaceKeyWasDown = FALSE; 
		} 
	} 
}

void Draw_GameClear() 
{
	HDC hdc; char szBuffer[128]; 
	Draw_Rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 100, 100, 0); 

	if (SUCCEEDED(g_pDDSBack->GetDC(&hdc))) { 
		SetBkMode(hdc, TRANSPARENT); 
		SetTextColor(hdc, RGB(255, 255, 255)); 
		TextOut(hdc, SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT / 2 - 80, "おめでとう！", lstrlen("おめでとう！")); 
		TextOut(hdc, SCREEN_WIDTH / 2 - 160, SCREEN_HEIGHT / 2 - 50, "すべてのステージをクリアしました！", lstrlen("すべてのステージをクリアしました！")); 
		wsprintf(szBuffer, "最終スコア：%d", g_dwScore); 
		TextOut(hdc, SCREEN_WIDTH / 2 - 90, SCREEN_HEIGHT / 2 - 20, szBuffer, lstrlen(szBuffer)); 
		TextOut(hdc, SCREEN_WIDTH / 2 - 125, SCREEN_HEIGHT / 2 + 30, "スペースキーで タイトルへ", lstrlen("スペースキーで タイトルへ")); 
		g_pDDSBack->ReleaseDC(hdc); 
	}

	Flip_To_Screen(); 
}

void Update_GameOver() 
{ 
	if (GetAsyncKeyState(VK_SPACE) & 0x8000) { 
		g_bSpaceKeyWasDown = TRUE; 
	}else{ 
		if (g_bSpaceKeyWasDown) { 
			g_eGameState = STATE_TITLE; g_bSpaceKeyWasDown = FALSE; 
		} 
	} 

	if (GetAsyncKeyState(VK_ESCAPE) & 0x8000){
		PostMessage(g_hwnd, WM_CLOSE, 0, 0); 
	}
}

void Draw_GameOver() 
{ 
	HDC hdc; char szBuffer[128]; 
	Draw_Rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 80, 0, 0); 

	if (SUCCEEDED(g_pDDSBack->GetDC(&hdc))) { 
		SetBkMode(hdc, TRANSPARENT); 
		SetTextColor(hdc, RGB(255, 0, 0)); 
		TextOut(hdc, SCREEN_WIDTH / 2 - 80, SCREEN_HEIGHT / 2 - 80, "ゲームオーバー", lstrlen("ゲームオーバー")); 
		SetTextColor(hdc, RGB(255, 255, 255)); 
		wsprintf(szBuffer, "最終スコア：%d", g_dwScore); TextOut(hdc, SCREEN_WIDTH / 2 - 110, SCREEN_HEIGHT / 2 - 40, szBuffer, lstrlen(szBuffer)); 
		wsprintf(szBuffer, "ハイスコア：%d", g_dwHighScore); TextOut(hdc, SCREEN_WIDTH / 2 - 90, SCREEN_HEIGHT / 2 - 20, szBuffer, lstrlen(szBuffer)); 
		TextOut(hdc, SCREEN_WIDTH / 2 - 125, SCREEN_HEIGHT / 2 + 30, "スペースキーで タイトルへ", lstrlen("スペースキーで タイトルへ")); 
		TextOut(hdc, SCREEN_WIDTH / 2 - 115, SCREEN_HEIGHT / 2 + 60, "ESCキーで ゲームをおわる", lstrlen("ESCキーで ゲームをおわる")); 
		g_pDDSBack->ReleaseDC(hdc); 
	} 
	
	Flip_To_Screen(); 
}

//=============================================================================
// ◆ ユーティリティ関数 (補助的な便利関数)
//=============================================================================
void Draw_Rect(int x, int y, int w, int h, int r, int g, int b) 
{ 
	HDC hdc; 

	if (SUCCEEDED(g_pDDSBack->GetDC(&hdc))) { 

		RECT rc = { x, y, x + w, y + h }; 

		COLORREF color = RGB(r, g, b); 
		HBRUSH hBrush = CreateSolidBrush(color); 
		FillRect(hdc, &rc, hBrush); 
		DeleteObject(hBrush); 
		g_pDDSBack->ReleaseDC(hdc); 

	}
}

//=============================================================================
// ◆ ステージ開始・ゲームリセットの処理 (★壁の初期配置を修正)
//=============================================================================
void StartNextStage()
{
    int i;
    g_nCurrentStage++;
    if ((DWORD)g_nCurrentStage >= MAX_STAGES)
    {
        if (g_dwScore > g_dwHighScore) { g_dwHighScore = g_dwScore; }
        g_eGameState = STATE_GAME_CLEAR;
    }
    else
    {
        g_dwCurrentStageScore = 0;
        g_Player.state = PSTATE_NORMAL;
        g_Player.x = 100; g_Player.y = GROUND_Y - PLAYER_SIZE; g_Player.vy = 0; g_Player.onGround = TRUE;
        
        int current_x = 0;
        for(i=0; i < NUM_GROUND_SEGMENTS; i++)
        {
            g_GroundSegments[i].x = (float)current_x;
            g_GroundSegments[i].isPit = FALSE;
            g_GroundSegments[i].width = 200 + rand() % 100;
            current_x += g_GroundSegments[i].width;
        }

        g_Obstacles[0].active = TRUE; g_Obstacles[0].scored = FALSE; g_Obstacles[0].height = 30 + rand() % 50;
        g_Obstacles[0].x = (float)SCREEN_WIDTH + 100.0f;
        for (i = 1; i < MAX_OBSTACLES; i++)
        {
            g_Obstacles[i].active = TRUE; g_Obstacles[i].scored = FALSE; g_Obstacles[i].height = 30 + rand() % 50;
            g_Obstacles[i].x = g_Obstacles[i-1].x + (float)(250 + rand() % 150);
        }
        g_eGameState = STATE_PLAYING;
    }
}

void Reset_Game()
{
    g_dwScore = 0; 
	g_dwCurrentStageScore = 0; 
	g_nPlayerLives = 3; 
	g_nCurrentStage = -1; 
	g_bSpaceKeyWasDown = TRUE; 
	StartNextStage();
}


