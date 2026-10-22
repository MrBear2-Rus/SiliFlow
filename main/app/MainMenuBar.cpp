#include "MainMenuBar.h"

#include <wx/menu.h>

namespace sf
{

wxMenuBar* CreateMainMenuBar()
{
    auto* bar = new wxMenuBar();

    // ---------------------------------------------------------------- 文件
    auto* file = new wxMenu;
    file->Append(wxID_NEW, "新建工程\tCtrl+N", "新建一个空工程");
    file->Append(wxID_OPEN, "打开工程…\tCtrl+O", "打开 .sfproj 工程文件");
    file->Append(wxID_SAVE, "保存\tCtrl+S", "保存当前工程");
    file->Append(wxID_SAVEAS, "另存为…\tCtrl+Shift+S", "另存为新的 .sfproj");
    file->AppendSeparator();
    file->Append(ID_FILE_OPEN_VCD, "打开 VCD 波形…", "加载仿真波形文件（M5 里程碑）");
    file->AppendSeparator();
    file->Append(wxID_EXIT, "退出\tAlt+F4", "退出 SiliFlow");
    bar->Append(file, "文件(&F)");

    // ---------------------------------------------------------------- 编辑
    auto* edit = new wxMenu;
    edit->Append(wxID_UNDO, "撤销\tCtrl+Z");
    edit->Append(wxID_REDO, "重做\tCtrl+Y");
    edit->AppendSeparator();
    edit->Append(wxID_CUT, "剪切\tCtrl+X");
    edit->Append(wxID_COPY, "复制\tCtrl+C");
    edit->Append(wxID_PASTE, "粘贴\tCtrl+V");
    edit->Append(wxID_DELETE, "删除\tDel");
    edit->AppendSeparator();
    edit->Append(wxID_SELECTALL, "全选\tCtrl+A");
    edit->AppendSeparator();
    edit->Append(wxID_PREFERENCES, "偏好设置…");
    bar->Append(edit, "编辑(&E)");

    // ---------------------------------------------------------------- 视图
    auto* view = new wxMenu;
    view->Append(ID_VIEW_ZOOM_IN, "放大\tCtrl+=");
    view->Append(ID_VIEW_ZOOM_OUT, "缩小\tCtrl+-");
    view->Append(ID_VIEW_ZOOM_FIT, "适应窗口\tCtrl+0");
    view->Append(ID_VIEW_ZOOM_RESET, "实际大小 100%\tCtrl+1");
    view->AppendSeparator();
    view->AppendCheckItem(ID_VIEW_SHOW_GRID, "显示网格", "切换网格显示");
    view->AppendCheckItem(ID_VIEW_SHOW_PIN_NAMES, "显示引脚名");
    view->AppendCheckItem(ID_VIEW_SHOW_LABELS, "显示元件标签");
    view->AppendSeparator();

    auto* panels = new wxMenu;
    panels->AppendCheckItem(ID_PANEL_TOOLBOX, "元件库");
    panels->AppendCheckItem(ID_PANEL_PROPERTY, "属性");
    panels->AppendCheckItem(ID_PANEL_TREE, "层次树");
    panels->AppendCheckItem(ID_PANEL_WAVE, "波形");
    panels->AppendCheckItem(ID_PANEL_CONSOLE, "消息");
    view->AppendSubMenu(panels, "面板");
    bar->Append(view, "视图(&V)");

    // ---------------------------------------------------------------- 工具
    auto* tools = new wxMenu;
    tools->AppendRadioItem(ID_TOOL_DRAG, "拖拽\tEsc 退出", "平移画布");
    tools->AppendRadioItem(ID_TOOL_SELECT, "选择\tS", "点选 / 框选 / 拖动");
    tools->AppendRadioItem(ID_TOOL_TEXT, "文本\tT", "添加注释或修改标签");
    tools->AppendRadioItem(ID_TOOL_COMPONENT, "元件\tC", "放置元件");
    tools->AppendRadioItem(ID_TOOL_WIRE, "导线\tW", "连接引脚");
    tools->AppendRadioItem(ID_TOOL_DRAWING, "绘图\tD", "画装饰图形");
    tools->AppendRadioItem(ID_TOOL_ERASER, "擦除\tE", "点擦 / 框擦");
    tools->AppendSeparator();

    auto* drawMenu = new wxMenu;
    drawMenu->AppendRadioItem(ID_DRAW_LINE, "直线");
    drawMenu->AppendRadioItem(ID_DRAW_RECTANGLE, "矩形");
    drawMenu->AppendRadioItem(ID_DRAW_CIRCLE, "圆形");
    tools->AppendSubMenu(drawMenu, "绘图形状");
    tools->AppendSeparator();
    tools->Append(ID_TOOL_ROTATE_CW, "顺时针旋转 90°\tCtrl+R");
    tools->Append(ID_TOOL_ROTATE_CCW, "逆时针旋转 90°\tCtrl+Shift+R");
    tools->Append(ID_TOOL_MIRROR, "水平镜像\tCtrl+M");
    bar->Append(tools, "工具(&T)");

    // ---------------------------------------------------------------- 仿真
    auto* sim = new wxMenu;
    sim->Append(ID_SIM_RUN, "运行\tF5", "开始自动节拍");
    sim->Append(ID_SIM_STOP, "停止\tF6", "停止自动节拍");
    sim->AppendSeparator();
    sim->Append(ID_SIM_STEP, "单步\tCtrl+T", "走一个时钟拍（对齐 Logisim）");
    sim->Append(ID_SIM_RESET, "复位\tCtrl+Shift+T", "清空强制电平与 tick 计数");
    sim->AppendSeparator();

    auto* simFreq = new wxMenu;
    simFreq->AppendRadioItem(ID_SIM_FREQ_1, "1 Hz");
    simFreq->AppendRadioItem(ID_SIM_FREQ_2, "2 Hz");
    simFreq->AppendRadioItem(ID_SIM_FREQ_5, "5 Hz");
    simFreq->AppendRadioItem(ID_SIM_FREQ_10, "10 Hz");
    simFreq->AppendRadioItem(ID_SIM_FREQ_20, "20 Hz");
    sim->AppendSubMenu(simFreq, "节拍频率");

    sim->AppendSeparator();
    sim->Append(ID_SIM_TRUTH_TABLE, "生成真值表…\tCtrl+U", "按输入输出端口穷举（S3 里程碑）");
    bar->Append(sim, "仿真(&M)");

    // ---------------------------------------------------------------- 图纸
    auto* sheet = new wxMenu;
    sheet->Append(ID_SHEET_NEW, "新建图纸…");
    sheet->Append(ID_SHEET_CLOSE, "关闭当前图纸");
    bar->Append(sheet, "图纸(&H)");

    // ---------------------------------------------------------------- Verilog
    auto* verilog = new wxMenu;
    verilog->Append(ID_VERILOG_EXPORT, "导出 Verilog\tF7", "把当前图纸导出为 .v（M3 里程碑）");
    verilog->Append(ID_VERILOG_IMPORT, "从 Verilog 导入\tF8", "读取 .v 并重建图纸（M4 里程碑）");
    verilog->AppendSeparator();
    verilog->Append(ID_VERILOG_SYNC_STATUS, "双向同步状态…");
    verilog->Append(ID_VERILOG_OPEN_FILE, "打开对侧文件");
    bar->Append(verilog, "Verilog(&L)");

    // ---------------------------------------------------------------- 帮助
    auto* help = new wxMenu;
    help->Append(ID_HELP_SHORTCUTS, "快捷键一览\tF1");
    help->AppendSeparator();
    help->Append(wxID_ABOUT, "关于 SiliFlow");
    // 注意别用 (&H)：「图纸」已经占了 Alt+H
    bar->Append(help, "帮助(&A)");

    return bar;
}

}  // namespace sf
