[English version is here.](README.md)

## LEGO Printer (日本語版)
レゴブロックのEV3を使って作られた本格的な二色プリンター(ペンプロッター)です。

**免責事項** 本サイトは、LEGO®／レゴ®の商標所有者であるレゴグループの承認・許可・スポンサー契約を得て運営しているものではありません。

動画URL: (制作中...)

![Printer Image](media/img1.png)

このリポジトリは主に次の3つの要素から成ります。
* プリンターの設計図 ([Studio 2.0](https://www.bricklink.com/v3/studio/download.page) の作業データ)
* EV3上で動くプログラム ([EV3RT](https://toppers.jp/trac_user/ev3pf/wiki/WhatsEV3RT) で開発)
* LPPE (LEGO Pen Printer(Plotter) Editor)

## プリンターの設計図
`design/` ディレクトリ内に [Studio 2.0](https://www.bricklink.com/v3/studio/download.page) の作業ファイルが入っています。

## EV3上で動くプログラム
[EV3RT](https://toppers.jp/trac_user/ev3pf/wiki/WhatsEV3RT) のバージョン β7-3 で開発しました。

>[!CAUTION]
最新版では関数の引数の単位がミリ秒からマイクロ秒に変わっているため注意が必要です。最新版を使わなかった理由は、最新版に比べて β7-3 のほうが経験的に見て動作が安定していたからです。  

環境構築は以下のブログを参考にするのがおすすめ：  
https://qiita.com/koushiro/items/26aca0cb10c1d34c03a1

### ビルド方法
`ev3/` ディレクトリ内の `printer` ディレクトリを EV3RTを環境構築するときに作った `(略)/hrp2/sdk/workspace/` ディレクトリの中にコピーしてから、コマンドライン上で `(略)/hrp2/sdk/workspace/` ディレクトリに移動した状態で以下のコマンドを実行するとビルドできます：  
``$ make app=printer``

## LPPE (LEGO Pen Printer(Plotter) Editor)
LPPEは、EV3にテキストファイル形式でペンの軌道を伝えるために、画像を処理してペンの軌道を表すテキストファイルに変換する自作ソフトです。WSL上でしかテストしていないのですが、以下の要素があれば動作すると思います。
### 必要なツール・ライブラリ等 (これより下のバージョンでも動作するかもしれない)
* C++ 20
* CMake
* OpenGL
* OpenCV 4.7.0+
### ビルド方法
`lppe/` ディレクトリ内で以下のコマンドを実行してビルド用のディレクトリを生成+ビルド  
``$ sh ./reload.sh``  

ビルドしたあとは次のコマンドで実行可能：  
``$ ./lppe``  

二回目以降は、新しいファイルを作らない限りは次のコマンドでビルドできる：  
``$ sh ./build.sh``

## 追記
[Canva](https://www.canva.com) などで使える **超極細ゴシック体** というフォントが印刷用の画像を作るときに非常に使い勝手が良いです。

## ライセンス
本プロジェクトは、コンテンツの種類に応じて異なるライセンスが適用されます。

### ソフトウェア / ソースコード
`ev3/` および `lppe/` ディレクトリ内のすべてのプログラムファイル（`.cpp` `.hpp` `.c` `.h` `.cfg` `.sh` `.json` `CMakeLists.txt`）は、**MIT License** の下で公開されています。
詳細は [LICENSE.txt](LICENSE.txt) ファイルを参照してください。

### プリンター設計データ
`design/` ディレクトリ内の Studio 2.0 プロジェクトファイル（`.io`）および `media/` ディレクトリ内のすべての画像ファイルは、**クリエイティブ・コモンズ 表示 - 非営利 4.0 国際 (CC BY-NC 4.0)** の下で公開されています。
* **表示**: 本プロジェクトのクレジット（リポジトリへのリンク等）を表示する必要があります。
* **非営利**: 本設計データを営利目的で利用することはできません。

詳細は [CC BY-NC 4.0](https://creativecommons.org/licenses/by-nc/4.0/deed.ja) を参照してください。

## 謝辞

本プロジェクトでは、以下のオープンソースライブラリおよびアセットを使用しています。利便性のため、一部のソースコードやアセットは各ライセンスに基づきリポジトリ内に含まれています。

* [EV3RT](https://toppers.jp/trac_user/ev3pf/wiki/WhatsEV3RT) - [TOPPERS ライセンス](https://www.toppers.jp/license.html)：
  >Copyright (C) 2000-2003 by Embedded and Real-Time Systems Laboratory<br>
  >                            Toyohashi Univ. of Technology, JAPAN<br>
  >Copyright (C) 2004-2010 by Embedded and Real-Time Systems Laboratory<br>
  >            Graduate School of Information Science, Nagoya Univ., JAPAN<br>
  >本ソフトウェアは，無保証で提供されているものである．上記著作権者およびTOPPERSプロジェクトは，本ソフトウェアに関して，特定の使用目的に対する適合性も含めて，いかなる保証も行わない．また，本ソフトウェアの利用により直接的または間接的に生じたいかなる損害に関しても，その責任を負わない．
* [ImGui](https://github.com/ocornut/imgui) - MIT License (`lppe/extern/imgui/` に同梱)
* [glad](https://github.com/Dav1dde/glad) - MIT License (`lppe/extern/glad/` に同梱)
* [Google Fonts](https://fonts.google.com/) (Roboto, NotoSansJP) - SIL Open Font License 1.1 (`lppe/assets/fonts/` に同梱)
* [Material Icons](https://fonts.google.com/icons) - Apache License 2.0 (`lppe/assets/images/` に同梱。ただし `default.jpg` を除く)

また、本プロジェクトの一部のコードは、AI（Gemini / ChatGPT / Copilot等）による補助を受けて作成されました。
