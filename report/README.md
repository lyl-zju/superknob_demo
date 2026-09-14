# SuperKnob 项目报告

## 文件说明

- `main.tex`：封面、摘要、第 1 章、第 9 章与总目录结构
- `sections/section_2_3_platform.tex`：第 2--3 章，硬件平台与软件平台
- `sections/section_4_6_core.tex`：第 4--6 章，FOC、四种手感与 FreeRTOS 双核通信
- `sections/section_7_modules.tex`：第 7 章，各功能模块
- `sections/section_8_process.tex`：第 8 章，设计、制作、调试与结果
- `projectreport.sty`：参照指定报告工程整理的版式文件
- `figures/`：封面所需校名与校徽图片
- `main.pdf`：已编译的报告

## 编译方法

在本目录运行：

```powershell
latexmk -xelatex -interaction=nonstopmode -halt-on-error main.tex
```

报告已完成项目简介、软硬件平台、FOC 原理及应用、四种手感、FreeRTOS 双核通信等章节。功能模块、设计制作调试与结果、项目总结、个人体会和成员分工均保留了待补充位置。
