[日本語版はこちら (Japanese version)](README_JP.md)

## LEGO Printer
A high-quality dual-color printer (pen plotter) built with LEGO® MINDSTORMS® EV3.

**Disclaimer** LEGO® is a trademark of the LEGO Group of companies which does not sponsor, authorize or endorse this site.

Video: (coming soon...)

![Printer Image](media/img1.png)

This repository consists of three main components:
* Printer Design : [Studio 2.0](https://www.bricklink.com/v3/studio/download.page) project files.
* EV3 Program : Programs developed for the [EV3RT](https://toppers.jp/trac_user/ev3pf/wiki/WhatsEV3RT) platform.
* LPPE (LEGO Pen Printer(Plotter) Editor)

## Printer Design
The [Studio 2.0](https://www.bricklink.com/v3/studio/download.page) project files are located in the `design/` directory.

## EV3 Program
This program was developed using [EV3RT](https://toppers.jp/trac_user/ev3pf/wiki/WhatsEV3RT) version β7-3.
> [!CAUTION]
Please note that in newer versions, the time units for certain function arguments have changed from milliseconds to microseconds. I chose β7-3 over the latest version because, in my experience, its behavior is more stable for this project.

For environment setup, I recommend following this guide (Japanese):
https://qiita.com/koushiro/items/26aca0cb10c1d34c03a1

### How to Build
1. Copy the `printer` directory in `ev3/` to your `(path)/hrp2/sdk/workspace/` directory.
2. Navigate to `(path)/hrp2/sdk/workspace/` directory in your terminal and run the following command:<br>
`$ make app=printer`

## LPPE (LEGO Pen Printer(Plotter) Editor)
LPPE is a custom tool that processes images and converts them into trajectory data (text files) for the EV3. While it has only been tested on WSL, it should work in environments that meet the following requirements.
### Requirements
* C++ 20
* CMake
* OpenGL
* OpenCV 4.7.0+ (May work on older versions, but not guaranteed)
### How to Build

1. To generate the build directory and compile for the first time, run the following in the `lppe/` directory:<br>
``$ sh ./reload.sh``  
2. Once built, you can run the application with:<br>
``$ ./lppe``  
3. For subsequent builds (without adding new files), you can use:<br>
``$ sh ./build.sh``

## Trivia
When creating images for printing, the font **"超極細ゴシック体"** (available on [Canva](https://www.canva.com) etc.) is highly recommended for its clean, ultra-thin lines.

## License
This project is licensed under different terms depending on the content:

### Software / Source Code
All program files (`.cpp` `.hpp` `.c` `.h` `.cfg` `.sh` `.json` `CMakeLists.txt`) in the `ev3/` and `lppe/` directories are licensed under the **MIT License**.
See the [LICENSE.txt](LICENSE.txt) file for the full text.

### Printer Design Assets & Media
The Studio 2.0 project files (`.io`) in the `design/` directory and all images in the `media/` directory are licensed under **Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)**.
* **Attribution**: You must give appropriate credit (link to this repository).
* **Non-Commercial**: You may not use this design for commercial purposes.

For more details, see [CC BY-NC 4.0](https://creativecommons.org/licenses/by-nc/4.0/).

## Acknowledgments
This project uses the following open-source libraries and assets:
* [EV3RT](https://toppers.jp/trac_user/ev3pf/wiki/WhatsEV3RT) - [TOPPERS License](https://www.toppers.jp/license.html) :
  >Copyright (C) 2000-2003 by Embedded and Real-Time Systems Laboratory<br>
  >                            Toyohashi Univ. of Technology, JAPAN<br>
  >Copyright (C) 2004-2010 by Embedded and Real-Time Systems Laboratory<br>
  >            Graduate School of Information Science, Nagoya Univ., JAPAN<br>
  >本ソフトウェアは，無保証で提供されているものである．上記著作権者およびTOPPERSプロジェクトは，本ソフトウェアに関して，特定の使用目的に対する適合性も含めて，いかなる保証も行わない．また，本ソフトウェアの利用により直接的または間接的に生じたいかなる損害に関しても，その責任を負わない．
* [ImGui](https://github.com/ocornut/imgui) - MIT License (included in `lppe/extern/imgui/`)
* [glad](https://github.com/Dav1dde/glad) - MIT License (included in `lppe/extern/glad/`)
* [Google Fonts](https://fonts.google.com/) (Roboto, NotoSansJP) - SIL Open Font License 1.1 (included in `lppe/assets/fonts`)
* [Material Icons](https://fonts.google.com/icons) - Apache License 2.0 (included in `lppe/assets/images/`, excluding `default.jpg`)

Parts of the code in this project were developed with the assistance of AI tools (Gemini / ChatGPT / Copilot).
