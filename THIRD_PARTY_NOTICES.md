# 第三方组件说明 / Third-party notices

Tmd 自有代码与文档采用 MIT；以下组件保留其原有授权。安装包和便携包中的 `licenses/` 提供授权文本与组件清单。

Tmd code and original documentation use MIT. The following components keep their original licenses. Distributed packages include license texts and component inventories in `licenses/`.

| Component | Use | License/source |
| --- | --- | --- |
| Qt 6.11.1 Core, Gui, Widgets, Network, Svg and required plugins | Application framework and rendering | LGPL-3.0 and the component-specific licenses in the bundled SPDX inventories; [Qt licensing](https://doc.qt.io/qt-6/licensing.html) |
| Open Sans | Embedded document font | SIL Open Font License 1.1; `licenses/open-sans-LICENSE.txt` |
| DejaVu Sans Mono | Embedded code font | Bitstream Vera / DejaVu notices; `licenses/DejaVuSansMono_LICENSE` |
| MinGW-w64 / GCC runtime DLLs | Windows compiler runtime | GCC runtime licensing and exception; see the upstream sources below |
| Inno Setup 6.7.3 | Installer and uninstaller | Inno Setup license; `licenses/Inno-Setup-LICENSE.txt` |
| Simplified Chinese installer translation | Installer language | Maintained by Zhenghan Yang and distributed by [Inno Setup](https://jrsoftware.org/files/istrans/); original header retained |

Qt 以共享 DLL 方式分发。用户可以替换接口兼容的 Qt 库，并为调试这些修改进行相应分析。本项目没有限制这类修改的额外条款。字体原始版权与授权说明随包保留。

Qt is distributed as shared DLLs. Users may replace them with interface-compatible modified libraries and debug those modifications. Tmd adds no restriction on those activities. Original font copyright and license notices are included.

Corresponding upstream sources:

- Qt Base 6.11.1: <https://github.com/qt/qtbase/tree/v6.11.1>
- Qt SVG 6.11.1: <https://github.com/qt/qtsvg/tree/v6.11.1>
- Qt source archives: <https://download.qt.io/official_releases/qt/6.11/6.11.1/submodules/>
- GCC 13.1: <https://gcc.gnu.org/releases.html> and <https://gcc.gnu.org/onlinedocs/libstdc++/manual/license.html>
- MinGW-w64: <https://www.mingw-w64.org/>
- Inno Setup 6.7.3: <https://github.com/jrsoftware/issrc/tree/is-6_7_3>

The bundled Qt `qtbase-6.11.1.spdx.json` and `qtsvg-6.11.1.spdx.json` inventories identify component versions, copyright notices and third-party licensing. Qt's internal libraries keep the individual license texts in `licenses/`; the MIT license for Tmd does not replace them.
