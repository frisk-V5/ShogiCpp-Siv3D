//=============================================================================
//  将棋アプリ プロトタイプ  (OpenSiv3D v0.6 系)
//  単一ファイル構成 ─ シンプル & 拡張しやすいオブジェクト指向
//=============================================================================
# include <Siv3D.hpp>

//─────────────────────────────────────────────────────────────────────────────
// 定数
//─────────────────────────────────────────────────────────────────────────────
namespace Const
{
    constexpr int   BOARD_SIZE   = 9;          // 盤のマス数 (縦横共通)
    constexpr int   CELL_SIZE    = 68;         // 1マスのピクセルサイズ
    constexpr int   BOARD_OFFSET_X = 60;       // 盤面描画の左上 X
    constexpr int   BOARD_OFFSET_Y = 40;       // 盤面描画の左上 Y
    constexpr int   HAND_AREA_W  = 160;        // 持ち駒エリアの幅
}

//─────────────────────────────────────────────────────────────────────────────
// 陣営
//─────────────────────────────────────────────────────────────────────────────
enum class Side { None, Black /*先手(下)*/, White /*後手(上)*/ };

//─────────────────────────────────────────────────────────────────────────────
// 駒の種類
//─────────────────────────────────────────────────────────────────────────────
enum class PieceType
{
    None,
    Pawn,        // 歩
    Lance,       // 香
    Knight,      // 桂
    Silver,      // 銀
    Gold,        // 金
    Bishop,      // 角
    Rook,        // 飛
    King,        // 王/玉
    // ── 成り駒 ──
    ProPawn,     // と
    ProLance,    // 成香
    ProKnight,   // 成桂
    ProSilver,   // 成銀
    ProBishop,   // 馬
    ProRook,     // 龍
};

//─────────────────────────────────────────────────────────────────────────────
// 駒の漢字表記を返すユーティリティ
//─────────────────────────────────────────────────────────────────────────────
String pieceLabel(PieceType t)
{
    switch (t)
    {
        case PieceType::Pawn:      return U"歩";
        case PieceType::Lance:     return U"香";
        case PieceType::Knight:    return U"桂";
        case PieceType::Silver:    return U"銀";
        case PieceType::Gold:      return U"金";
        case PieceType::Bishop:    return U"角";
        case PieceType::Rook:      return U"飛";
        case PieceType::King:      return U"王";
        case PieceType::ProPawn:   return U"と";
        case PieceType::ProLance:  return U"杏";
        case PieceType::ProKnight: return U"圭";
        case PieceType::ProSilver: return U"全";
        case PieceType::ProBishop: return U"馬";
        case PieceType::ProRook:   return U"龍";
        default:                   return U"";
    }
}

//─────────────────────────────────────────────────────────────────────────────
// 駒 1 個を表す構造体
//─────────────────────────────────────────────────────────────────────────────
struct Piece
{
    PieceType type = PieceType::None;
    Side      side = Side::None;
};

//─────────────────────────────────────────────────────────────────────────────
// 盤面クラス  ─  9×9 の二次元配列でマスを管理する
//─────────────────────────────────────────────────────────────────────────────
class Board
{
public:
    // grid[row][col]  row=0 が上(後手側), row=8 が下(先手側)
    std::array<std::array<Piece, 9>, 9> grid{};

    // 盤面を初期配置にリセットする
    void initialize()
    {
        // まず全マスをクリア
        for (auto& row : grid)
            for (auto& cell : row)
                cell = { PieceType::None, Side::None };

        // ── 後手(White) の初期配置 ──
        // 1段目 (row=0)
        const std::array<PieceType, 9> backRow = {
            PieceType::Lance, PieceType::Knight, PieceType::Silver,
            PieceType::Gold,  PieceType::King,   PieceType::Gold,
            PieceType::Silver,PieceType::Knight, PieceType::Lance
        };
        for (int c = 0; c < 9; ++c)
            grid[0][c] = { backRow[c], Side::White };

        // 2段目 (row=1): 角・飛
        grid[1][1] = { PieceType::Bishop, Side::White };
        grid[1][7] = { PieceType::Rook,   Side::White };

        // 3段目 (row=2): 歩×9
        for (int c = 0; c < 9; ++c)
            grid[2][c] = { PieceType::Pawn, Side::White };

        // ── 先手(Black) の初期配置 ──
        // 9段目 (row=8)
        for (int c = 0; c < 9; ++c)
            grid[8][c] = { backRow[8 - c], Side::Black };   // 左右反転

        // 8段目 (row=7): 飛・角
        grid[7][7] = { PieceType::Bishop, Side::Black };
        grid[7][1] = { PieceType::Rook,   Side::Black };

        // 7段目 (row=6): 歩×9
        for (int c = 0; c < 9; ++c)
            grid[6][c] = { PieceType::Pawn, Side::Black };
    }

    // (row, col) が盤内かを返す
    bool inBounds(int row, int col) const
    {
        return (row >= 0 && row < 9 && col >= 0 && col < 9);
    }

    // 駒を移動させる (移動ルールチェックなし)
    void movePiece(int fromRow, int fromCol, int toRow, int toCol)
    {
        grid[toRow][toCol]   = grid[fromRow][fromCol];
        grid[fromRow][fromCol] = { PieceType::None, Side::None };
    }
};

//─────────────────────────────────────────────────────────────────────────────
// 選択状態を管理する構造体
//─────────────────────────────────────────────────────────────────────────────
struct Selection
{
    bool selected = false;
    int  row = -1;
    int  col = -1;

    void clear() { selected = false; row = col = -1; }
};

//─────────────────────────────────────────────────────────────────────────────
// 描画 & 操作を担うゲームクラス
//─────────────────────────────────────────────────────────────────────────────
class ShogiGame
{
public:
    ShogiGame()
        : m_font(FontMethod::MSDF, 36, Typeface::CJK_Regular_JP)   // 日本語フォント
        , m_smallFont(FontMethod::MSDF, 18, Typeface::CJK_Regular_JP)
    {
        m_board.initialize();   // 初期配置をセット
    }

    // メインループから毎フレーム呼ぶ
    void update()
    {
        handleMouseInput();
    }

    void draw() const
    {
        drawBoardBackground();
        drawGrid();
        drawPieces();
        drawSelection();
        drawUI();
    }

private:
    //── データ ──────────────────────────────────────────────────
    Board     m_board;
    Selection m_sel;
    Font      m_font;
    Font      m_smallFont;
    Side      m_currentTurn = Side::Black;   // 先手から開始

    //── ヘルパー: マウス座標 → マス (row, col) 変換 ─────────────
    // 盤内なら true を返し row/col に値をセット
    bool screenToCell(const Point& pos, int& row, int& col) const
    {
        int x = pos.x - Const::BOARD_OFFSET_X;
        int y = pos.y - Const::BOARD_OFFSET_Y;
        col = x / Const::CELL_SIZE;
        row = y / Const::CELL_SIZE;
        return (x >= 0 && y >= 0 &&
                col >= 0 && col < Const::BOARD_SIZE &&
                row >= 0 && row < Const::BOARD_SIZE);
    }

    // マス (row, col) の左上スクリーン座標を返す
    Point cellOrigin(int row, int col) const
    {
        return {
            Const::BOARD_OFFSET_X + col * Const::CELL_SIZE,
            Const::BOARD_OFFSET_Y + row * Const::CELL_SIZE
        };
    }

    //── マウス入力処理 ──────────────────────────────────────────
    void handleMouseInput()
    {
        if (!MouseL.down()) return;   // 左クリックのみ処理

        int row, col;
        if (!screenToCell(Cursor::Pos(), row, col)) return;   // 盤外は無視

        const Piece& clicked = m_board.grid[row][col];

        if (!m_sel.selected)
        {
            // ── 選択フェーズ: 自分の駒をクリックした場合のみ選択 ──
            if (clicked.side == m_currentTurn)
            {
                m_sel.selected = true;
                m_sel.row = row;
                m_sel.col = col;
            }
        }
        else
        {
            // ── 移動フェーズ ──
            if (row == m_sel.row && col == m_sel.col)
            {
                // 同じマスをクリック → 選択解除
                m_sel.clear();
            }
            else if (clicked.side == m_currentTurn)
            {
                // 自分の別の駒をクリック → 選択し直し
                m_sel.row = row;
                m_sel.col = col;
            }
            else
            {
                // 空きマス or 相手の駒 → 移動実行
                m_board.movePiece(m_sel.row, m_sel.col, row, col);
                m_sel.clear();
                // ターン交代
                m_currentTurn = (m_currentTurn == Side::Black)
                                ? Side::White : Side::Black;
            }
        }
    }

    //── 描画: 盤の背景 ─────────────────────────────────────────
    void drawBoardBackground() const
    {
        // 盤全体の背景(畳色)
        const int totalW = Const::CELL_SIZE * Const::BOARD_SIZE;
        const int totalH = Const::CELL_SIZE * Const::BOARD_SIZE;
        Rect{ Const::BOARD_OFFSET_X, Const::BOARD_OFFSET_Y, totalW, totalH }
            .draw(ColorF{ 0.82, 0.67, 0.43 })          // 木目色
            .drawFrame(3, 0, Palette::Sienna);          // 外枠
    }

    //── 描画: グリッド線 ───────
