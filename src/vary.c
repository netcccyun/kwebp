#include <string.h>
#include <ctype.h>
#include "ksapi.h"

static bool qvalue_is_positive(const char* value, const char* end)
{
	bool saw_digit = false;
	bool positive = false;
	while (value < end && isspace((unsigned char)*value)) ++value;
	while (end > value && isspace((unsigned char)end[-1])) --end;
	if (value == end) return false;
	if (*value == '1') {
		++value;
		if (value == end) return true;
		if (*value++ != '.') return false;
		while (value < end) {
			if (*value++ != '0') return false;
			saw_digit = true;
		}
		return saw_digit;
	}
	if (*value == '0') {
		++value;
	} else if (*value != '.') {
		return false;
	}
	if (value < end && *value == '.') ++value;
	while (value < end) {
		if (!isdigit((unsigned char)*value)) return false;
		saw_digit = true;
		if (*value != '0') positive = true;
		++value;
	}
	return saw_digit && positive;
}

bool is_support_webp(const char* accept, int len)
{
	const char* p;
	const char* end;
	if (accept == NULL || len <= 0) return false;
	p = accept;
	end = accept + len;
	while (p < end) {
		const char* item_end = (const char*)memchr(p, ',', (size_t)(end - p));
		const char* media_end;
		const char* params;
		if (item_end == NULL) item_end = end;
		while (p < item_end && isspace((unsigned char)*p)) ++p;
		media_end = p;
		while (media_end < item_end && *media_end != ';') ++media_end;
		params = media_end;
		while (media_end > p && isspace((unsigned char)media_end[-1])) --media_end;
		if ((size_t)(media_end - p) == sizeof("image/webp") - 1 &&
			strncasecmp(p, "image/webp", sizeof("image/webp") - 1) == 0) {
			bool allowed = true;
			while (params < item_end) {
				const char* param_end;
				const char* value;
				++params;
				while (params < item_end && isspace((unsigned char)*params)) ++params;
				param_end = params;
				while (param_end < item_end && *param_end != ';') ++param_end;
				if (param_end - params >= 2 && (params[0] == 'q' || params[0] == 'Q') && params[1] == '=') {
					value = params + 2;
					while (value < param_end && isspace((unsigned char)*value)) ++value;
					allowed = qvalue_is_positive(value, param_end);
				}
				params = param_end;
			}
			if (allowed) return true;
		}
		p = item_end < end ? item_end + 1 : end;
	}
	return false;
}
static bool response_vary(kgl_vary_context* ctx, const char* value)
{
	ctx->write(ctx->cn, kgl_expand_string("Accept"));
	return true;
}
static bool build_vary(kgl_vary_context* ctx, const char* value)
{
	char buf[512];
	DWORD size = sizeof(buf);
	buf[0] = '\0';
	KGL_RESULT result = ctx->get_variable(ctx->cn, KGL_VAR_HEADER, "Accept", buf, &size);
	if (result == KGL_OK) {
		if (is_support_webp(buf, size)) {
			ctx->write(ctx->cn, kgl_expand_string("webp"));
		}
	}
	return true;
}
static kgl_vary vary = {
	"webp",
	build_vary,
	response_vary,
};
void register_vary(kgl_dso_version* ver)
{
	KGL_REGISTER_VARY(ver, &vary);
}
