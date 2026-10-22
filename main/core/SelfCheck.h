#pragma once

// =============================================================================
//  SiliFlow · core/SelfCheck.h
//  数据层自检：不需要 GUI，命令行加 --selfcheck 即触发。
//  这是 M1 里程碑的验收手段，也方便在没有窗口环境时验证 core 是否正常。
// =============================================================================

#include <wx/string.h>

namespace sf
{

/// 跑一遍「建库 → 建图纸 → 放元件 → 连线 → JSON 往返 → 校验」。
/// 返回 0 表示全部通过；report 里是逐项结果。
int RunCoreSelfCheck(wxString* report);

}  // namespace sf
