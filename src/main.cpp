//=============================================================================
//  将棋アプリ プロトタイプ v2  (OpenSiv3D v0.6 系)
//
//  実装済み機能:
//    - 駒の移動ベクトルをデータテーブルで定義
//    - 走り駒（飛・角・香）の射線チェック
//    - 移動可能マスのハイライト
//    - 持ち駒の管理と「打つ」操作
//    - 成りダイアログ (SimpleGUI)
//    - 二歩・打ち歩詰め禁止 (基本チェック)
//    - 盤座標の将棋記法(1〜9, 一〜九)対応
//=============================================================================
# include <Siv3D.hpp>

//─────────────────────────────────────────────────────────────────────────────
// 定数
//─────────────────────────────────────────────────────────────────────────────
namespace Const
{
    constexpr int BOARD_SIZE    = 9;
    constexpr int CELL          = 68;       // 1マスのピクセルサイズ
    constexpr int BX            = 150;      // 盤面描画の左上 X
    constexpr int BY            = 40;       // 盤面描画の左上 Y
    constexpr int HAND_W        = 130;      // 駒台エリアの幅
    constexpr int HAND_BLACK_X  = 10;      // 先手駒台の X
    constexpr int HAND_WHITE_X  = BX + CELL * BOARD_SIZE + 10;  // 後手駒台の X
}

//─────────────────────────────────────────────────────────────────────────────
// 陣営
//─────────────────────────────────────────────────────────────────────────────
enum class Side : uint8 { None, Black /*先手(下)*/, White /*後手(上)*/ };

//─────────────────────────────────────────────────────────────────────────────
// 駒の種類
//─────────────────────────────────────────────────────────────────────────────
enum class PieceType : uint8
{
    None,
    Pawn,       // 歩
    Lance,      // 香
    Knight,     // 桂
    Silver,     // 銀
    Gold,       // 金
    Bishop,     // 角
    Rook,       // 飛
    King,       // 王/玉
    // ── 成り駒 ──
    ProPawn,    // と
    ProLance,   // 成香
    ProKnight,  // 成桂
    ProSilver,  // 成銀
    ProBishop,  // 馬
    ProRook,    // 龍
};

// 成れる駒かどうか
bool canPromote(PieceType t)
{
    return (t == PieceType::Pawn   || t == PieceType::Lance  ||
            t == PieceType::Knight || t == PieceType::Silver ||
            t == PieceType::Bishop || t == PieceType::Rook);
}

// 成り変換
PieceType promoted(PieceType t)
{
    switch (t)
    {
        case PieceType::Pawn:   return PieceType::ProPawn;
        case PieceType::Lance:  return PieceType::ProLance;
        case PieceType::Knight: return PieceType::ProKnight;
        case PieceType::Silver: return PieceType::ProSilver;
        case PieceType::Bishop: return PieceType::ProBishop;
        case PieceType::Rook:   return PieceType::ProRook;
        default:                return t;
    }
}

// 持ち駒に戻す（成りを解除）
PieceType demoted(PieceType t)
{
    switch (t)
    {
        case PieceType::ProPawn:   return PieceType::Pawn;
        case PieceType::ProLance:  return PieceType::Lance;
        case PieceType::ProKnight: return PieceType::Knight;
        case PieceType::ProSilver: return PieceType::Silver;
        case PieceType::ProBishop: return PieceType::Bishop;
        case PieceType::ProRook:   return PieceType::Rook;
        default:                   return t;
    }
}

// 駒の漢字表記
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
// 駒の移動ベクトルテーブル
//   Point{ dc, dr } : (列方向の変化, 行方向の変化)
//   先手(Black)視点。後手は dr を反転して使う。
//   "slider" フラグが true の駒は壁に当たるまで繰り返す。
//─────────────────────────────────────────────────────────────────────────────
struct MoveEntry
{
    Array<Point> steps;    // 単歩移動ベクトル (先手視点)
    Array<Point> sliders;  // 射線移動ベクトル (飛・角・香など)
};

// 金将と同じ動きをまとめるヘルパー
static const Array<Point> GOLD_STEPS = {
    {0,-1},{1,-1},{-1,-1},{1,0},{-1,0},{0,1}
};

static const HashTable<PieceType, MoveEntry> MOVE_TABLE = {
    // 歩: 前1マス
    { PieceType::Pawn,
      { {{0,-1}}, {} } },

    // 香: 前方に走る
    { PieceType::Lance,
      { {}, {{0,-1}} } },

    // 桂: 前方斜め2段跳び (飛び越え可)
    { PieceType::Knight,
      { {{-1,-2},{1,-2}}, {} } },

    // 銀: 斜め4方向 + 前
    { PieceType::Silver,
      { {{0,-1},{1,-1},{-1,-1},{1,1},{-1,1}}, {} } },

    // 金: 前後左右 + 斜め前
    { PieceType::Gold,
      { GOLD_STEPS, {} } },

    // 角: 斜め4方向に走る
    { PieceType::Bishop,
      { {}, {{1,-1},{-1,-1},{1,1},{-1,1}} } },

    // 飛: 縦横4方向に走る
    { PieceType::Rook,
      { {}, {{0,-1},{0,1},{1,0},{-1,0}} } },

    // 王: 8方向1マス
    { PieceType::King,
      { {{0,-1},{0,1},{1,0},{-1,0},{1,-1},{-1,-1},{1,1},{-1,1}}, {} } },

    // と(成歩): 金と同じ動き
    { PieceType::ProPawn,
      { GOLD_STEPS, {} } },

    // 成香: 金と同じ動き
    { PieceType::ProLance,
      { GOLD_STEPS, {} } },

    // 成桂: 金と同じ動き
    { PieceType::ProKnight,
      { GOLD_STEPS, {} } },

    // 成銀: 金と同じ動き
    { PieceType::ProSilver,
      { GOLD_STEPS, {} } },

    // 馬(成角): 斜めに走る + 縦横1マス
    { PieceType::ProBishop,
      { {{0,-1},{0,1},{1,0},{-1,0}},
        {{1,-1},{-1,-1},{1,1},{-1,1}} } },

    // 龍(成飛): 縦横に走る + 斜め1マス
    { PieceType::ProRook,
      { {{1,-1},{-1,-1},{1,1},{-1,1}},
        {{0,-1},{0,1},{1,0},{-1,0}} } },
};

//─────────────────────────────────────────────────────────────────────────────
// 駒構造体
//─────────────────────────────────────────────────────────────────────────────
struct Piece
{
    PieceType type = PieceType::None;
    Side      side = Side::None;
    bool isEmpty() const { return type == PieceType::None; }
};

//─────────────────────────────────────────────────────────────────────────────
// 盤クラス  9×9 の二次元配列
//   座標系: grid[row][col]
//   row=0 が上辺(後手側 = 9段目相当), row=8 が下辺(先手側 = 1段目)
//   col=0 が左端(9筋), col=8 が右端(1筋)
//─────────────────────────────────────────────────────────────────────────────
class Board
{
public:
    std::array<std::array<Piece, 9>, 9> grid{};

    // 初期配置
    void initialize()
    {
        for (auto& row : grid)
            for (auto& c : row)
                c = {};

        // 後手 (White) 陣
        const Array<PieceType> back = {
            PieceType::Lance, PieceType::Knight, PieceType::Silver,
            PieceType::Gold,  PieceType::King,   PieceType::Gold,
            PieceType::Silver,PieceType::Knight, PieceType::Lance
        };
        for (int c = 0; c < 9; ++c) grid[0][c] = { back[c],          Side::White };
        grid[1][1] = { PieceType::Bishop, Side::White };
        grid[1][7] = { PieceType::Rook,   Side::White };
        for (int c = 0; c < 9; ++c) grid[2][c] = { PieceType::Pawn,  Side::White };

        // 先手 (Black) 陣
        for (int c = 0; c < 9; ++c) grid[8][c] = { back[8-c],        Side::Black };
        grid[7][7] = { PieceType::Bishop, Side::Black };
        grid[7][1] = { PieceType::Rook,   Side::Black };
        for (int c = 0; c < 9; ++c) grid[6][c] = { PieceType::Pawn,  Side::Black };
    }

    bool inBounds(int r, int c) const
    {
        return r >= 0 && r < 9 && c >= 0 && c < 9;
    }

    Piece& at(int r, int c)       { return grid[r][c]; }
    const Piece& at(int r, int c) const { return grid[r][c]; }
};

//─────────────────────────────────────────────────────────────────────────────
// 移動判定エンジン
//   側: side が先手なら dr はそのまま、後手なら dr を反転する
//─────────────────────────────────────────────────────────────────────────────
class MoveEngine
{
public:
    // 指定した駒が移動できるマスの一覧を返す
    static Array<Point> legalDestinations(
        const Board& board, int fromR, int fromC)
    {
        const Piece& p = board.at(fromR, fromC);
        if (p.isEmpty()) return {};

        const int sign = (p.side == Side::Black) ? 1 : -1;  // 後手は方向反転
        Array<Point> result;

        auto it = MOVE_TABLE.find(p.type);
        if (it == MOVE_TABLE.end()) return {};
        const MoveEntry& entry = it->second;

        // 単歩移動
        for (const auto& d : entry.steps)
        {
            int nr = fromR + d.y * sign;
            int nc = fromC + d.x;
            if (!board.inBounds(nr, nc)) continue;
            const Piece& target = board.at(nr, nc);
            if (target.side != p.side)   // 味方の駒がなければOK
                result.emplace_back(nc, nr);
        }

        // 射線移動 (飛・角・香など)
        for (const auto& d : entry.sliders)
        {
            int nr = fromR + d.y * sign;
            int nc = fromC + d.x;
            while (board.inBounds(nr, nc))
            {
                const Piece& target = board.at(nr, nc);
                if (target.side == p.side) break;       // 味方 → 止まる
                result.emplace_back(nc, nr);
                if (!target.isEmpty()) break;            // 相手 → 取って止まる
                nr += d.y * sign;
                nc += d.x;
            }
        }

        return result;
    }

    // 打ち駒が置けるマスを返す
    static Array<Point> legalDrops(
        const Board& board, PieceType type, Side side)
    {
        Array<Point> result;
        const int sign = (side == Side::Black) ? 1 : -1;

        for (int r = 0; r < 9; ++r)
        {
            for (int c = 0; c < 9; ++c)
            {
                if (!board.at(r, c).isEmpty()) continue;

                // 歩・香は最奥段に打てない
                if (type == PieceType::Pawn || type == PieceType::Lance)
                {
                    int dest = (side == Side::Black) ? 0 : 8;
                    if (r == dest) continue;
                }
                // 桂馬は最奥2段に打てない
                if (type == PieceType::Knight)
                {
                    if (side == Side::Black && r <= 1) continue;
                    if (side == Side::White && r >= 7) continue;
                }
                // 二歩チェック
                if (type == PieceType::Pawn && hasOwnPawnInCol(board, side, c))
                    continue;

                result.emplace_back(c, r);
            }
        }
        return result;
    }

    // 敵陣 (成れる領域) かどうか
    static bool isEnemyTerritory(int row, Side side)
    {
        return (side == Side::Black && row <= 2) ||
               (side == Side::White && row >= 6);
    }

    // 必ず成らなければならないケース (行き場がない)
    static bool mustPromote(PieceType type, int toRow, Side side)
    {
        if (type == PieceType::Pawn || type == PieceType::Lance)
            return (side == Side::Black) ? (toRow == 0) : (toRow == 8);
        if (type == PieceType::Knight)
            return (side == Side::Black) ? (toRow <= 1) : (toRow >= 7);
        return false;
    }

private:
    // その列に同じ側の歩があるか (二歩判定)
    static bool hasOwnPawnInCol(const Board& board, Side side, int col)
    {
        for (int r = 0; r < 9; ++r)
        {
            const auto& p = board.at(r, col);
            if (p.side == side && p.type == PieceType::Pawn)
                return true;
        }
        return false;
    }
};

//─────────────────────────────────────────────────────────────────────────────
// 持ち駒管理
//─────────────────────────────────────────────────────────────────────────────
struct Hand
{
    std::map<PieceType, int> pieces;  // 駒の種類 → 枚数

    void add(PieceType t)
    {
        PieceType base = demoted(t);  // 成り駒は元に戻す
        ++pieces[base];
    }

    bool has(PieceType t) const
    {
        auto it = pieces.find(t);
        return it != pieces.end() && it->second > 0;
    }

    void remove(PieceType t)
    {
        if (pieces.count(t) && pieces[t] > 0)
            --pieces[t];
    }

    // 持ち駒リスト (枚数 > 0 のもの)
    Array<PieceType> list() const
    {
        Array<PieceType> result;
        for (const auto& [type, cnt] : pieces)
            if (cnt > 0) result.push_back(type);
        return result;
    }
};

//─────────────────────────────────────────────────────────────────────────────
// 選択状態
//─────────────────────────────────────────────────────────────────────────────
enum class SelectMode { None, BoardPiece, HandPiece };

struct Selection
{
    SelectMode  mode      = SelectMode::None;
    int         row       = -1;
    int         col       = -1;
    PieceType   handType  = PieceType::None;  // 持ち駒から選んだ場合
    Array<Point> moves;   // 移動可能マス (col, row)

    void clear()
    {
        mode     = SelectMode::None;
        row      = col = -1;
        handType = PieceType::None;
        moves.clear();
    }
};

//─────────────────────────────────────────────────────────────────────────────
// 成りダイアログの状態
//─────────────────────────────────────────────────────────────────────────────
struct PromotionDialog
{
    bool      active   = false;
    int       toRow    = -1;
    int       toCol    = -1;
    PieceType origType = PieceType::None;
    Side      side     = Side::None;

    void open(int r, int c, PieceType t, Side s)
    {
        active = true; toRow = r; toCol = c; origType = t; side = s;
    }
    void close() { active = false; }
};

//─────────────────────────────────────────────────────────────────────────────
// ゲーム本体
//─────────────────────────────────────────────────────────────────────────────
class ShogiGame
{
public:
    ShogiGame()
        : m_font(FontMethod::MSDF, 36, Typeface::CJK_Regular_JP)
        , m_labelFont(FontMethod::MSDF, 20, Typeface::CJK_Regular_JP)
        , m_smallFont(FontMethod::MSDF, 15, Typeface::CJK_Regular_JP)
    {
        m_board.initialize();
    }

    void update()
    {
        // 成りダイアログが開いている間は他の操作を受け付けない
        if (m_promDialog.active)
        {
            handlePromotionDialog();
            return;
        }
        if (MouseL.down())
            handleMouseClick(Cursor::Pos());
    }

    void draw() const
    {
        drawBackground();
        drawGrid();
        drawCoordLabels();
        drawHighlights();
        drawPieces();
        drawHandAreas();
        drawStatusBar();
        if (m_promDialog.active)
            drawPromotionDialog();
    }

private:
    Board           m_board;
    Hand            m_blackHand;   // 先手の持ち駒
    Hand            m_whiteHand;   // 後手の持ち駒
    Selection       m_sel;
    PromotionDialog m_promDialog;
    Side            m_turn   = Side::Black;
    String          m_status = U"先手の番です";
    Font            m_font;
    Font            m_labelFont;
    Font            m_smallFont;

    // 現在の手番の持ち駒
    Hand& currentHand()
    {
        return (m_turn == Side::Black) ? m_blackHand : m_whiteHand;
    }
    const Hand& currentHand() const
    {
        return (m_turn == Side::Black) ? m_blackHand : m_whiteHand;
    }

    //── 座標変換 ──────────────────────────────────────────────────
    // スクリーン座標 → 盤のマス (row, col) 変換
    Optional<std::pair<int,int>> screenToCell(const Point& pos) const
    {
        int x = pos.x - Const::BX;
        int y = pos.y - Const::BY;
        if (x < 0 || y < 0) return none;
        int c = x / Const::CELL;
        int r = y / Const::CELL;
        if (c >= 9 || r >= 9) return none;
        return std::make_pair(r, c);
    }

    // マスの左上スクリーン座標
    Point cellOrigin(int row, int col) const
    {
        return { Const::BX + col * Const::CELL, Const::BY + row * Const::CELL };
    }

    // マスの中心スクリーン座標
    Vec2 cellCenter(int row, int col) const
    {
        auto o = cellOrigin(row, col);
        return { o.x + Const::CELL / 2.0, o.y + Const::CELL / 2.0 };
    }

    //── 持ち駒台のクリック判定 ─────────────────────────────────────
    // 返値: クリックされた駒の種類 (None なら台外)
    Optional<PieceType> handPieceAt(const Point& pos, Side side) const
    {
        int baseX = (side == Side::Black) ? Const::HAND_BLACK_X : Const::HAND_WHITE_X;
        const Hand& hand = (side == Side::Black) ? m_blackHand : m_whiteHand;
        auto types = hand.list();
        int startY = (side == Side::Black) ? 400 : 40;

        for (int i = 0; i < (int)types.size(); ++i)
        {
            Rect cell{ baseX, startY + i * 50, Const::HAND_W, 46 };
            if (cell.contains(pos))
                return types[i];
        }
        return none;
    }

    //── マウスクリック処理 ─────────────────────────────────────────
    void handleMouseClick(const Point& pos)
    {
        // 持ち駒台のクリックを先にチェック
        if (auto t = handPieceAt(pos, m_turn))
        {
            // 持ち駒を選択
            if (currentHand().has(*t))
            {
                m_sel.clear();
                m_sel.mode     = SelectMode::HandPiece;
                m_sel.handType = *t;
                m_sel.moves    = MoveEngine::legalDrops(m_board, *t, m_turn);
            }
            return;
        }

        // 盤面クリック
        auto cell = screenToCell(pos);
        if (!cell) { m_sel.clear(); return; }
        auto [r, c] = *cell;

        if (m_sel.mode == SelectMode::None)
        {
            // 選択フェーズ: 自分の駒を選ぶ
            const Piece& p = m_board.at(r, c);
            if (p.side == m_turn)
                selectBoardPiece(r, c);
        }
        else if (m_sel.mode == SelectMode::BoardPiece)
        {
            handleBoardSelection(r, c);
        }
        else if (m_sel.mode == SelectMode::HandPiece)
        {
            handleHandDrop(r, c);
        }
    }

    void selectBoardPiece(int r, int c)
    {
        m_sel.clear();
        m_sel.mode = SelectMode::BoardPiece;
        m_sel.row  = r;
        m_sel.col  = c;
        m_sel.moves = MoveEngine::legalDestinations(m_board, r, c);
    }

    // 盤面の駒を選択中 → 移動先をクリックした
    void handleBoardSelection(int r, int c)
    {
        // 同じマスをクリック → 解除
        if (r == m_sel.row && c == m_sel.col) { m_sel.clear(); return; }

        // 自分の別の駒をクリック → 選択し直し
        if (m_board.at(r, c).side == m_turn) { selectBoardPiece(r, c); return; }

        // 移動可能マスかチェック
        bool legal = false;
        for (const auto& mv : m_sel.moves)
            if (mv.x == c && mv.y == r) { legal = true; break; }
        if (!legal) { m_sel.clear(); return; }

        // 移動を実行
        execMove(m_sel.row, m_sel.col, r, c);
    }

    // 持ち駒を選択中 → 打つ先をクリックした
    void handleHandDrop(int r, int c)
    {
        bool legal = false;
        for (const auto& mv : m_sel.moves)
            if (mv.x == c && mv.y == r) { legal = true; break; }
        if (!legal) { m_sel.clear(); return; }

        // 打つ
        m_board.at(r, c) = { m_sel.handType, m_turn };
        currentHand().remove(m_sel.handType);
        m_sel.clear();
        switchTurn();
    }

    // 移動実行 (取り + 成り判定)
    void execMove(int fr, int fc, int tr, int tc)
    {
        Piece& from   = m_board.at(fr, fc);
        Piece& target = m_board.at(tr, tc);

        // 取った駒を持ち駒に加える
        if (!target.isEmpty())
            currentHand().add(target.type);

        // 移動
        target = from;
        from   = {};
        m_sel.clear();

        // 成り判定
        bool inEnemy  = MoveEngine::isEnemyTerritory(tr, m_turn);
        bool mustProm = MoveEngine::mustPromote(target.type, tr, m_turn);
        bool canProm  = canPromote(target.type) && inEnemy;

        if (mustProm)
        {
            // 強制成り
            target.type = promoted(target.type);
            switchTurn();
        }
        else if (canProm)
        {
            // 成りダイアログを開く
            m_promDialog.open(tr, tc, target.type, m_turn);
        }
        else
        {
            switchTurn();
        }
    }

    // 成りダイアログの操作
    void handlePromotionDialog()
    {
        // ダイアログは drawPromotionDialog() で SimpleGUI を使う
        // ボタン押下は draw 側から直接処理するため、ここでは何もしない
    }

    void switchTurn()
    {
        m_turn   = (m_turn == Side::Black) ? Side::White : Side::Black;
        m_status = (m_turn == Side::Black) ? U"先手の番です" : U"後手の番です";
    }

    //── 描画: 背景 ─────────────────────────────────────────────────
    void drawBackground() const
    {
        Scene::Rect().draw(ColorF{ 0.18, 0.14, 0.10 });
    }

    //── 描画: 盤グリッド ────────────────────────────────────────────
    void drawGrid() const
    {
        int W = Const::CELL * 9;
        int H = Const::CELL * 9;
        Rect{ Const::BX, Const::BY, W, H }
            .draw(ColorF{ 0.80, 0.65, 0.40 })
            .drawFrame(4, 0, ColorF{ 0.40, 0.25, 0.10 });

        for (int i = 1; i < 9; ++i)
        {
            int x = Const::BX + i * Const::CELL;
            int y = Const::BY + i * Const::CELL;
            Line{ x, Const::BY,    x, Const::BY + H }.draw(1.5, ColorF{0.40,0.25,0.10,0.7});
            Line{ Const::BX, y, Const::BX + W, y }.draw(1.5, ColorF{0.40,0.25,0.10,0.7});
        }

        // 星 (3-3, 3-7, 7-3, 7-7) — row/col は 3,6 に当たる
        for (int r : {2, 5}) for (int c : {2, 5})
        {
            auto o = cellOrigin(r + 1, c + 1);
            Circle{ (double)o.x, (double)o.y, 4 }.draw(ColorF{0.30,0.15,0.05});
        }
    }

    //── 描画: 座標ラベル (1〜9, 一〜九) ──────────────────────────
    void drawCoordLabels() const
    {
        const Array<String> kanji = {
            U"一",U"二",U"三",U"四",U"五",U"六",U"七",U"八",U"九"
        };
        for (int i = 0; i < 9; ++i)
        {
            // 列番号: 右から 9, 8, ... 1
            int col = 8 - i;
            int num = i + 1;
            double cx = Const::BX + col * Const::CELL + Const::CELL / 2.0;
            m_smallFont(num).drawAt(Vec2{ cx, (double)(Const::BY - 16) }, Palette::Burlywood);

            // 段ラベル
            double ry = Const::BY + i * Const::CELL + Const::CELL / 2.0;
            m_smallFont(kanji[i]).drawAt(
                Vec2{ (double)(Const::BX + 9 * Const::CELL + 18), ry },
                Palette::Burlywood);
        }
    }

    //── 描画: 移動可能マスのハイライト ─────────────────────────────
    void drawHighlights() const
    {
        if (m_sel.mode == SelectMode::None) return;

        // 選択中のマスを青枠
        if (m_sel.mode == SelectMode::BoardPiece)
        {
            auto o = cellOrigin(m_sel.row, m_sel.col);
            Rect{ o.x, o.y, Const::CELL, Const::CELL }
                .drawFrame(4, 0, ColorF{ 0.2, 0.7, 1.0 });
        }

        // 移動可能マスを半透明の緑で塗る
        for (const auto& mv : m_sel.moves)
        {
            auto o = cellOrigin(mv.y, mv.x);
            Rect{ o.x, o.y, Const::CELL, Const::CELL }
                .draw(ColorF{ 0.0, 0.9, 0.3, 0.30 })
                .drawFrame(2, 0, ColorF{ 0.0, 0.9, 0.3, 0.7 });
        }
    }

    //── 描画: 全駒 ─────────────────────────────────────────────────
    void drawPieces() const
    {
        for (int r = 0; r < 9; ++r)
            for (int c = 0; c < 9; ++c)
            {
                const Piece& p = m_board.at(r, c);
                if (!p.isEmpty())
                    drawOnePiece(cellOrigin(r, c), p);
            }
    }

    // 1枚の駒を描画する
    void drawOnePiece(const Point& origin, const Piece& p) const
    {
        const int cs = Const::CELL;
        const bool isWhite = (p.side == Side::White);
        const Color fill   = isWhite ? Color{235,220,190} : Color{255,248,220};
        const Color border = Palette::Saddlebrown;
        const Color text   = isWhite ? Color{160,0,0}     : Palette::Black;

        int mg = 5;
        // 五角形 (先手: 上が尖る / 後手: 下が尖る)
        Quad shape;
        if (!isWhite)
        {
            shape = Quad{
                Vec2{(double)(origin.x+mg),    (double)(origin.y+cs-mg)},
                Vec2{(double)(origin.x+cs-mg), (double)(origin.y+cs-mg)},
                Vec2{(double)(origin.x+cs-mg), (double)(origin.y+mg+10)},
                Vec2{(double)(origin.x+cs/2),  (double)(origin.y+mg)}
            };
        }
        else
        {
            shape = Quad{
                Vec2{(double)(origin.x+mg),    (double)(origin.y+mg)},
                Vec2{(double)(origin.x+cs-mg), (double)(origin.y+mg)},
                Vec2{(double)(origin.x+cs-mg), (double)(origin.y+cs-mg-10)},
                Vec2{(double)(origin.x+cs/2),  (double)(origin.y+cs-mg)}
            };
        }
        shape.draw(fill).drawFrame(1.5, border);

        Vec2 center{ origin.x + cs/2.0, origin.y + cs/2.0 };
        if (isWhite)
        {
            const Transformer2D t{ Mat3x2::Rotate(Math::Pi, center) };
            m_font(pieceLabel(p.type)).drawAt(center, text);
        }
        else
        {
            m_font(pieceLabel(p.type)).drawAt(center, text);
        }
    }

    //── 描画: 駒台 ─────────────────────────────────────────────────
    void drawHandAreas() const
    {
        drawOneHandArea(m_whiteHand, Side::White, 40);
        drawOneHandArea(m_blackHand, Side::Black, 400);
    }

    void drawOneHandArea(const Hand& hand, Side side, int startY) const
    {
        int bx = (side == Side::Black) ? Const::HAND_BLACK_X : Const::HAND_WHITE_X;
        const String title = (side == Side::Black) ? U"▼先手\n持ち駒" : U"▲後手\n持ち駒";

        // 台の背景
        Rect{ bx, startY - 30, Const::HAND_W, 360 }
            .rounded(8).draw(ColorF{0.25,0.20,0.12}).drawFrame(2, ColorF{0.6,0.45,0.25});
        m_smallFont(title).draw(bx + 4, startY - 28, Palette::Burlywood);

        auto types = hand.list();
        for (int i = 0; i < (int)types.size(); ++i)
        {
            PieceType t  = types[i];
            int        cnt = hand.pieces.at(t);
            int        iy  = startY + i * 50;
            bool       sel = (m_sel.mode == SelectMode::HandPiece &&
                              m_sel.handType == t && side == m_turn);

            Rect cell{ bx, iy, Const::HAND_W, 46 };
            cell.rounded(6)
                .draw(sel ? ColorF{0.6,0.9,1.0,0.25} : ColorF{0.55,0.40,0.20,0.4})
                .drawFrame(sel ? 3 : 1, sel ? ColorF{0.2,0.7,1.0} : ColorF{0.6,0.45,0.25});

            // 駒の絵と枚数
            Point po{ bx + 4, iy + 2 };
            drawOnePiece(po, {t, side});
            m_labelFont(U"×", cnt).draw(bx + Const::CELL + 8, iy + 12, Palette::Burlywood);
        }
    }

    //── 描画: ステータスバー ────────────────────────────────────────
    void drawStatusBar() const
    {
        m_labelFont(m_status).draw(Const::BX, Const::BY + 9 * Const::CELL + 10, Palette::Burlywood);
        m_smallFont(U"駒クリック→選択  移動先クリック→移動  持ち駒クリック→打つ")
            .draw(Const::BX, Const::BY + 9 * Const::CELL + 36, ColorF{0.6,0.6,0.6});
    }

    //── 描画 & 操作: 成りダイアログ ───────────────────────────────
    void drawPromotionDialog()
    {
        // 半透明オーバーレイ
        Scene::Rect().draw(ColorF{0,0,0,0.45});

        // ダイアログ本体
        Rect dlg{ 220, 260, 360, 160 };
        dlg.rounded(12).draw(ColorF{0.20,0.15,0.10}).drawFrame(3, ColorF{0.7,0.55,0.30});

        m_labelFont(U"成りますか？").drawAt(dlg.center() + Vec2{0,-44}, Palette::Burlywood);

        // 「成る」ボタン
        if (SimpleGUI::Button(U"成る", Vec2{ 240, 330 }, 140))
        {
            m_board.at(m_promDialog.toRow, m_promDialog.toCol).type
                = promoted(m_promDialog.origType);
            m_promDialog.close();
            switchTurn();
        }
        // 「成らない」ボタン
        if (SimpleGUI::Button(U"成らない", Vec2{ 390, 330 }, 140))
        {
            m_promDialog.close();
            switchTurn();
        }
    }
};

//─────────────────────────────────────────────────────────────────────────────
// Main
//─────────────────────────────────────────────────────────────────────────────
void Main()
{
    Window::Resize(820, 700);
    Window::SetTitle(U"将棋 v2  (OpenSiv3D)");
    Scene::SetBackground(ColorF{ 0.18, 0.14, 0.10 });

    ShogiGame game;

    while (System::Update())
    {
        game.update();
        game.draw();
    }
}
