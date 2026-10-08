#include "core/settings/MarkdownSettings.h"
const std::array<MarkdownNumberSpec, MarkdownSettings::NumberCount> &markdownNumberSpecs() {
    static const std::array<MarkdownNumberSpec, MarkdownSettings::NumberCount> specs{{
        {"正文字号", " pt", 60, 480, 10, 0}, {"代码字号", " pt", 60, 480, 10, 0},
        {"一级标题 H1", " pt", 60, 960, 10, 1}, {"二级标题 H2", " pt", 60, 960, 10, 1},
        {"三级标题 H3", " pt", 60, 960, 10, 1}, {"四级标题 H4", " pt", 60, 960, 10, 1},
        {"五级标题 H5", " pt", 60, 960, 10, 1}, {"六级标题 H6", " pt", 60, 960, 10, 1},
        {"正文行高", " %", 100, 250, 1, 2}, {"标题行高", " %", 100, 250, 1, 1},
        {"段落上下间距", " px", 0, 48, 1, 2}, {"标题段前间距", " px", 0, 80, 1, 1},
        {"标题段后间距", " px", 0, 80, 1, 1}, {"分点上下间距", " px", 0, 32, 1, 2},
        {"每级分点缩进", " px", 8, 96, 1, 2}, {"每级引用缩进", " px", 8, 80, 1, 2},
        {"代码块左右留白", " px", 0, 48, 1, 3}, {"代码块上下间距", " px", 0, 48, 1, 3},
        {"Tab 宽度", " px", 8, 128, 1, 3}, {"表格单元格留白", " px", 0, 32, 1, 4},
        {"编辑后预加载延迟", " ms", 150, 2000, 1, 4}
    }};
    return specs;
}
const std::array<MarkdownColorSpec, MarkdownSettings::ColorCount> &markdownColorSpecs() {
    static const std::array<MarkdownColorSpec, MarkdownSettings::ColorCount> specs{{
        {"文档背景", 0}, {"正文颜色", 0}, {"标题颜色", 1}, {"代码块背景（三个反引号）", 3},
        {"行内代码背景（单反引号）", 3}, {"引用背景", 2}, {"链接颜色", 4}, {"表格边框", 4},
        {"表格表头背景", 4}, {"表格偶数行背景", 4}, {"表格奇数行背景", 4}
    }};
    return specs;
}
qreal MarkdownSettings::number(Number key) const { return qreal(numbers[key]) / markdownNumberSpecs()[key].scale; }
