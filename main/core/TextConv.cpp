#include "TextConv.h"

#include <wx/strconv.h>

namespace sf
{

void UseUtf8NarrowStrings()
{
    // wxConvLibc 是 wxString 从 const char* 构造时用的默认转换；
    // wxConvCurrent 是 wxConvertMB2WX / wxConvertWX2MB 用的那个。
    // 两个都指向 UTF-8，源码里的中文字面量才能正确进到 wxString。
    wxConvLibcPtr = &wxConvUTF8;
    wxConvCurrent = &wxConvUTF8;
}

}  // namespace sf
