# AI開発ステーション - アーキテクチャ定義

## プロジェクトパス
`/Users/yamaguchinaoyuki/Desktop/JJJ/AAA`

## コアアーキテクチャ

### 1. GUIレイヤー (Node UI)
**Dear ImGui + imnodes** (C++製 - ネイティブGUI)

- **目的**: FlowiseライクなノードベースUI
- **利点**:
  - メモリ消費を最小化
  - M1チップ上で超高速描画
  - Metal APIネイティブ対応
  - Retina HiDPI完全対応
  - Pythonオーバーヘッドゼロ

### 2. 計算エンジン (Local AI)
**MLX C++ API** (Python経由なし)

- **目的**: ローカルLLM推論
- **利点**:
  - Unified Memory（64GB）フル活用
  - GIL制約なし
  - 最高速度のインファレンス
  - Apple Silicon最適化

### 3. ノード管理ロジック
**EnTT** (Entity Component System)

- **目的**: ノードの高速管理
- **構成**:
  - Entity: 各ノード（LLM、プロンプト、メモリ等）
  - Component: ノードの属性（位置、状態、データ）
  - System: ノード間の処理フロー

## 技術スタック

| レイヤー | 技術 | 役割 |
|---------|------|------|
| GUI | Dear ImGui + imnodes | ノードエディタUI |
| ECS | EnTT | ノード管理システム |
| AI推論 | MLX C++ API | ローカルLLM実行 |
| ビルド | CMake | クロスプラットフォームビルド |
| 描画 | Metal (macOS) / Vulkan (Linux) | GPU加速 |
| フォント | Noto Sans JP | 日本語対応 |

## モデル配置

```
models/
├── Holo1.5-3B/           # ビジョン対応LLM
├── Qwen2-VL-2B-Instruct-4bit/  # 軽量ビジョンLLM
├── Qwen2.5-7B-Instruct/  # メインLLM（7B）
└── moondream2-original/  # 軽量ビジョンモデル
```

## データフロー

```
┌─────────────────────────────────────────┐
│         Dear ImGui + imnodes            │
│      (ノードベースUI - C++ネイティブ)      │
└──────────────┬──────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────┐
│              EnTT (ECS)                 │
│    (ノード管理・グラフ実行エンジン)          │
└──────────────┬──────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────┐
│           MLX C++ API                   │
│    (Unified Memory 64GB フル活用)        │
│    (Apple Silicon GPU加速)              │
└─────────────────────────────────────────┘
```

## CP1要件の再定義

### タスク1.1: CMakeビルドシステム
- [x] .venv自動検出（不要 - Pythonを使わない）
- [ ] Dear ImGui / imnodes の統合
- [ ] EnTT の統合
- [ ] MLX C++ API の統合
- [ ] Metal フレームワークのリンク

### タスク1.2: 非同期実行（修正）
- [ ] ~~Thread-safe Python Manager~~ → **EnTT Job System**
- [ ] MLX推論の非同期実行
- [ ] GUIスレッドとの完全分離

### タスク1.3: Retina描画と日本語フォント
- [ ] Dear ImGui Metal バックエンド
- [ ] HiDPI スケーリング
- [ ] Noto Sans JP 統合

## 依存関係

### 必須ライブラリ
- **Dear ImGui**: v1.90+
- **imnodes**: latest
- **EnTT**: v3.13+
- **MLX**: v0.20+
- **GLFW**: v3.4+ (ウィンドウ管理)

### macOS専用
- **Metal**: Apple Silicon GPU
- **Cocoa**: ネイティブウィンドウ

### 開発ツール
- **CMake**: 3.18+
- **Clang**: 14+ (Apple Clang推奨)
- **Ninja**: 高速ビルド

## 重要な設計方針

1. **Pythonゼロ**: すべてC++で実装
2. **富豪的メモリ**: 64GB RAMをフル活用
3. **プロ仕様**: 妥協なし、手抜きなし
4. **60FPS維持**: すべての操作で即座に反応
5. **日本語徹底**: すべてのUIとメッセージ
