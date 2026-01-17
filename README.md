# AAAAA - AI開発ステーション

M1 Mac専用の次世代AI開発環境。Flowiseの概念をC++/EnTT/MLXで再定義した、世界最速の個人用AI開発ステーションです。

## プロジェクト概要

本プロジェクトは、M1 Mac 64GBという富豪的リソースを最大限活用し、以下を実現します：

1. **エージェント・オーケストレーション・エンジン**: AIエージェントの思考プロセスをノードベースで直感的に定義・変更
2. **ファイルシステム・ダイナミクス開発エンジン**: ソースコードを解析し、視覚的なノード構造へ自動展開する次世代IDE

## 技術スタック

- **GUI層**: Dear ImGui + Metal (Retina HiDPI対応)
- **管理層**: EnTT (ECS) + インメモリASTキャッシュ
- **推論層**: MLX C++ API (GPU直接活用)
- **ビルド基盤**: CMake + Python Embedding (pybind11)

## ビルド方法

### 1. 環境準備

```bash
# Python仮想環境の作成
uv sync

# ビルドディレクトリ作成
mkdir build && cd build
```

### 2. CMake設定とビルド

```bash
# CMake設定（.venvを自動検出）
cmake ..

# ビルド実行（M1最適化有効）
make -j$(sysctl -n hw.logicalcpu)
```

### 3. 実行

```bash
./core_app
```

## アーキテクチャの特徴

- **完全自動化**: ユーザーがパスを手動設定する必要なし
- **M1最適化**: `-mcpu=apple-m1`フラグによるApple Silicon Native実行
- **真の非同期**: GUIを操作しながら裏でPython処理が実行可能（GIL制御）
- **Retina完全対応**: DPIスケールを動的監視し、美しい日本語UI表示

## プロジェクト構造

```
AAAAA/
├── core/              # C++ Native ソースコード
│   ├── main.mm        # メインアプリケーション (ImGui + Metal)
│   ├── PythonManager  # 非同期Python実行管理
│   ├── FileScanner    # 高速ファイルスキャン
│   ├── ASTParser      # import文解析
│   └── GraphVisualizer # ノード描画
├── models/            # AI モデル格納
├── resources/         # フォント等のアセット
├── CMakeLists.txt     # ビルド設定
└── pyproject.toml     # Python依存関係
```

## ライセンス

本プロジェクトは個人開発用です。
