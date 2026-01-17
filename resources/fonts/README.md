# フォントリソース

AI開発ステーションで使用する日本語フォントを配置するディレクトリです。

## 推奨フォント: Noto Sans JP

### ダウンロード方法

#### 方法1: Google Fonts からダウンロード

```bash
# このディレクトリに移動
cd resources/fonts

# Noto Sans JP をダウンロード
curl -L "https://github.com/google/fonts/raw/main/ofl/notosansjp/NotoSansJP%5Bwght%5D.ttf" -o NotoSansJP-Regular.ttf
```

#### 方法2: システムフォントを使用（macOS）

macOS の場合、以下のシステムフォントが自動的に使用されます:
- ヒラギノ角ゴシック W3
- `/System/Library/Fonts/ヒラギノ角ゴシック W3.ttc`

#### 方法3: システムフォントを使用（Linux）

```bash
# Debian/Ubuntu
sudo apt install fonts-noto-cjk

# Arch Linux
sudo pacman -S noto-fonts-cjk

# Fedora
sudo dnf install google-noto-sans-cjk-fonts
```

## ライセンス

- **Noto Sans JP**: SIL Open Font License 1.1
  - 商用利用可
  - 改変可
  - 再配布可

## 使用方法

フォントファイルを `resources/fonts/` ディレクトリに配置すると、
AI開発ステーションが自動的に検出して使用します。

### 対応フォント形式

- `.ttf` (TrueType Font)
- `.otf` (OpenType Font)
- `.ttc` (TrueType Collection)

## フォールバック

フォントが見つからない場合、DearPyGuiのデフォルトフォントが使用されます。
ただし、日本語表示が正しく行われない可能性があります。
