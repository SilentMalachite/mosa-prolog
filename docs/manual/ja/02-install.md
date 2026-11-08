[English](../en/02-install.md) | 日本語

# 2. 入手と起動

この章では、Mosa Prolog をダウンロードして展開し、シェルから呼べるようにして、起動と終了をします。

## 始める前に

Mosa Prolog 0.1.0 が動くのは、次のコンピュータです。

- macOS 26 以降（Apple Silicon）
- Linux（x86_64）。glibc が Ubuntu 24.04 と同じか、それより新しいもの
- Windows 10 22H2 以降と Windows 11（x64）。Smart App Control が無効のもの

Mac の macOS がそれより古いときは、先に「システム設定」の「一般」にある「ソフトウェアアップデート」で macOS を新しくしてください。Apple Silicon の Mac なら、どれも macOS 26 に上げられます。

パッケージには署名がありません。OS が起動を止めたときの進め方を、手順 3（macOS）と手順 5（Windows）に書いています。Windows 11 には Smart App Control（日本語の画面では「スマート アプリ コントロール」）という設定があります。これが有効だと、Windows は署名のないプログラムを止めます。1 つのプログラムだけを通すこともできません。そのため、Smart App Control が有効な PC では Mosa は動かず、動作対象にも入れていません。設定は「Windows セキュリティ」の「アプリとブラウザー コントロール」で確かめられます。

`mosa` を動かすのに、ほかのものは要りません。C++ のコンパイラが要るのは `mosac`（第 13 章）だけです。

## 手順

1. Mosa Prolog の [Releases のページ](https://github.com/SilentMalachite/mosa-prolog/releases) を開き、リリース 0.1.0 から、自分のコンピュータに合うパッケージをダウンロードします。

| コンピュータ | パッケージ |
| --- | --- |
| macOS（Apple Silicon） | `mosa-0.1.0-macos-arm64.tar.gz` |
| Linux（x86_64） | `mosa-0.1.0-linux-x86_64.tar.gz` |
| Windows（x64） | `mosa-0.1.0-windows-x86_64.zip` |

2. パッケージをホームフォルダに展開します。どのパッケージにも、パッケージと同じ名前のフォルダが 1 つ入っていて、プログラムはその中の `bin` フォルダにあります。下のコマンドは、パッケージが「ダウンロード」フォルダにあるものとして書いています。macOS では「ターミナル」を開き（Command キーを押しながらスペースキーを押し、「ターミナル」と打って Enter を押します）、次の 2 行を打ちます。ターミナルが初めて「ダウンロード」フォルダを読むとき、macOS は「“ターミナル”から、“ダウンロード”フォルダ内のファイルへのアクセス権を求められています。」という確認を出します。許可する方のボタン（「OK」か「許可」）を押してください。「許可しない」を押すと、下の `tar` や `mv` の行がフォルダを読めません。「許可しない」を押してしまったときは、「システム設定」の「プライバシーとセキュリティ」から「ファイルとフォルダ」を開き、ターミナルの「“ダウンロード”フォルダ」をオンにします。

<!-- skip: unpack-macos -->
```console
$ cd ~
$ tar -xzf ~/Downloads/mosa-0.1.0-macos-arm64.tar.gz
```

Safari の設定で「ダウンロード後、“安全な”ファイルを開く」が有効だと、Safari がパッケージをもう展開していることがあります。「ダウンロード」フォルダに `mosa-0.1.0-macos-arm64` というフォルダがすでにあれば、`tar` は打ちません。代わりに `mv ~/Downloads/mosa-0.1.0-macos-arm64 ~` と打って、そのフォルダをホームフォルダへ移します。この場合も手順 3 は必要です。

Linux では端末を開き（Ubuntu では Ctrl-Alt-T を押します）、次のように打ちます。

<!-- skip: unpack-linux -->
```console
$ cd ~
$ tar -xzf ~/Downloads/mosa-0.1.0-linux-x86_64.tar.gz
```

Windows では PowerShell を開き（スタートメニューを開いて PowerShell と打ち、「Windows PowerShell」を選びます）、次の 1 行を打ちます。PowerShell は自分のプロンプト（`PS C:\Users\名前>`）を出すので、このマニュアルでは PowerShell の行にプロンプトを付けません。

<!-- skip: unpack-windows -->
```powershell
Expand-Archive "$HOME\Downloads\mosa-0.1.0-windows-x86_64.zip" -DestinationPath $HOME
```

3. macOS だけの手順です。隔離の印（quarantine）を外します。macOS は、Web ブラウザでダウンロードしたファイルにこの印を付けます。`tar` や Safari は、展開したファイルすべてにこの印を写します。印の付いた、署名のないプログラムを、macOS は「…を検証できません」のような画面を出して開きません。すでに `mosa` を起動しようとしてこの画面が出ていたら、「完了」か「キャンセル」の、画面にある方を押します。「ゴミ箱に入れる」は押さないでください。プログラムが消えてしまいます。次のコマンドは、フォルダ（`-d`）とその中のすべてのファイル（`-r`）から印を外します。

<!-- skip: quarantine-macos -->
```console
$ xattr -dr com.apple.quarantine ~/mosa-0.1.0-macos-arm64
```

4. `bin` フォルダを PATH に加えます。PATH は、コマンドの名前を打ったときにシェルが探すフォルダの一覧です。シェルが zsh の macOS では、次のように打ちます。

<!-- skip: path-macos -->
```console
$ echo 'export PATH="$HOME/mosa-0.1.0-macos-arm64/bin:$PATH"' >> ~/.zshrc
```

Linux のシェルは、多くの場合 bash です。次のように打ちます。

<!-- skip: path-linux -->
```console
$ echo 'export PATH="$HOME/mosa-0.1.0-linux-x86_64/bin:$PATH"' >> ~/.bashrc
```

Windows では、PowerShell で次の 3 行を打ちます。行頭の `$` も含めて、1 行ずつ全部を打ちます。PowerShell の `$bin` や `$old` は値を入れておく名前で、プロンプトではありません。

<!-- skip: path-windows -->
```powershell
$bin = "$HOME\mosa-0.1.0-windows-x86_64\bin"
$old = [Environment]::GetEnvironmentVariable("Path", "User")
[Environment]::SetEnvironmentVariable("Path", "$old;$bin", "User")
```

Windows では、画面を操作して PATH に加える方法もあります。スタートメニューで「環境変数」と検索し、自分のアカウントの環境変数を編集する項目を開きます。Path を選んで「編集」を押し、「新規」を押して、`bin` フォルダのパス `C:\Users\<名前>\mosa-0.1.0-windows-x86_64\bin` を打ちます。`<名前>` には自分のユーザー名を入れます。打ったら、その画面の「OK」を押し、「環境変数」の画面でも「OK」を押します。右上の × で閉じると、変更は捨てられます。どの OS でも、新しい PATH が効くのはこの後に開いたウィンドウだけです。ターミナルや PowerShell のウィンドウを閉じて、開き直してください。

5. シェルが Mosa を見つけられるか確かめます。`mosa --version` と打ちます。

<!-- check: version -->
```console
$ mosa --version
mosa 0.1.0
```

`mosa` が見つからないとシェルが言ったら、手順 4 のフォルダ名と、手順 2 でできたフォルダの名前を見比べてから、ウィンドウを開き直してください。Windows で `mosa` を起動したときに「Windows によって PC が保護されました」という青い画面（Microsoft Defender SmartScreen）が出たら、「詳細情報」を押し、それから「実行」を押します。

6. `mosa` だけを打って Mosa を起動します。Mosa はプロンプト `?- ` を出して、入力を待ちます。`halt.` と打って Enter を押すと終わります。

<!-- check: start-quit -->
```console
$ mosa
?- halt.
```

## 何が起きたか

展開したフォルダの `bin` には、`mosa`、`mosac`、`mosatest` と、これらが使うライブラリが入っています。フォルダはそのままの形で置いておいてください。`mosa` は自分の隣でライブラリを探すので、`mosa` だけをフォルダの外へ移すと、ライブラリが見つかりません。手順 4 でプログラムをどこかへ写さずに、フォルダを PATH に加えたのはこのためです。

`mosa --version` はバージョンを出して終わりました。`mosa` だけを打つと、対話の画面が始まります。Mosa は `?- ` を出して、問い合わせを待ちます。`halt.` は、この対話を終える問い合わせです。問い合わせは必ずピリオドで終わります。Mosa は、ピリオドの後で Enter を押したときに問い合わせを読みます。

入力を終わらせても対話は終わります。行の先頭で、macOS と Linux では Ctrl-D を押します。Windows では Ctrl-Z を押してから Enter を押します。Ctrl-D で終えたとき、Mosa は改行を出さずに終わります。bash（Linux で多いシェル）では、シェルのプロンプトが `?- ` のすぐ後ろに出ます。zsh（macOS の既定のシェル）では、そこに反転した `%` が出て、プロンプトは次の行に出ます。

## 練習問題

1. `mosa` を起動し、ピリオドを付けずに `halt` と打って Enter を押します。何が起きますか。その問い合わせを終えるにはどうしますか。
2. もう一度 `mosa` を起動し、`halt.` を打たずに終えてください。

### 答え

1. 何も起きないように見えます。問い合わせはピリオドまで打たないと終わらないので、Mosa は続きを待っています。`.` と打って Enter を押すと、Mosa は終わります。

<!-- check: no-period
halt
.
-->
```console
$ mosa
?- halt
.
```

2. Ctrl-D（macOS、Linux）か、Ctrl-Z の後に Enter（Windows）を押します。`halt.` の後と同じように、シェルに戻ります。

---

[目次](README.md) ・ 前: [1. はじめに](01-introduction.md) ・ 次: [3. 最初の問い合わせ](03-first-query.md)
