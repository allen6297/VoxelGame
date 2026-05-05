#include <quickjs.h>
#include <string>
#include <vector>

namespace voxel {

    std::vector<std::string> parseStringArray(JSContext* ctx, JSValueConst obj)
    {
        std::vector<std::string> result;

        if (!JS_IsArray(obj))
            return result;

        int64_t length = 0;
        if (JS_GetLength(ctx, obj, &length) < 0)
            return result;

        for (int64_t i = 0; i < length; ++i)
        {
            JSValue value = JS_GetPropertyUint32(ctx, obj, static_cast<uint32_t>(i));

            const char* str = JS_ToCString(ctx, value);
            if (str)
            {
                result.emplace_back(str);
                JS_FreeCString(ctx, str);
            }

            JS_FreeValue(ctx, value);
        }

        return result;
    }

} // namespace voxel