#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include "ksapi.h"
#include "mark.h"
#include "upstream.h"
#include "vary.h"
#define MAX_WEBP_SIZE 16777216
static bool parse_int(const char* text, int* value) {
	char* end = NULL;
	long parsed;
	if (text == NULL || *text == '\0') return false;
	errno = 0;
	parsed = strtol(text, &end, 10);
	if (errno != 0 || *end != '\0' || parsed < INT_MIN || parsed > INT_MAX) return false;
	*value = (int)parsed;
	return true;
}
static bool get_query_int(const char* query, const char* name, int* value) {
	size_t name_len = strlen(name);
	const char* p = query;
	while (p != NULL && *p != '\0') {
		const char* segment_end = strchr(p, '&');
		size_t segment_len = segment_end ? (size_t)(segment_end - p) : strlen(p);
		if (segment_len > name_len + 1 && memcmp(p, name, name_len) == 0 && p[name_len] == '=') {
			char number[32];
			size_t number_len = segment_len - name_len - 1;
			if (number_len == 0 || number_len >= sizeof(number)) return false;
			memcpy(number, p + name_len + 1, number_len);
			number[number_len] = '\0';
			return parse_int(number, value);
		}
		p = segment_end ? segment_end + 1 : NULL;
	}
	return false;
}
static void* create_ctx() {
	webp_mark* m = (webp_mark*)malloc(sizeof(webp_mark));
	if (m == NULL) return NULL;
	memset(m, 0, sizeof(webp_mark));
	m->quality = 75;
	if (!WebPConfigPreset(&m->config, WEBP_PRESET_DEFAULT, (float)m->quality)) {
		fprintf(stderr, "cann't init config\n");
	}
	return m;
}
static void free_ctx(void* ctx) {
	webp_mark* m = (webp_mark*)ctx;
	free(m);
}
static KGL_RESULT build(kgl_access_build* build_ctx, uint32_t build_type) {
	webp_mark* m = (webp_mark*)build_ctx->module;
	char buf[512];
	int len = sprintf(buf, "%d", m->quality);
	switch (build_type) {
	case 0:
		build_ctx->write_string(build_ctx->cn, buf, len, 0);
		break;
	case 1:
		build_ctx->write_string(build_ctx->cn, kgl_expand_string("quality:<input name='quality' value='"), 0);
		build_ctx->write_string(build_ctx->cn, buf, len, 0);
		build_ctx->write_string(build_ctx->cn, kgl_expand_string("'/>1 - 100(best)<br>"), 0);
		build_ctx->write_string(build_ctx->cn, kgl_expand_string("max:<input name='max' value='"), 0);
		len = sprintf(buf, "%d", m->max_length);
		build_ctx->write_string(build_ctx->cn, buf, len, 0);
		build_ctx->write_string(build_ctx->cn, kgl_expand_string("'/>(default:16M)"), 0);
		break;
	}
	return KGL_OK;
}
static KGL_RESULT parse(kgl_access_parse_config* parse_ctx) {
	webp_mark* m = (webp_mark*)parse_ctx->module;
	const char* quality = parse_ctx->body->get_value(parse_ctx->cn, "quality");
	if (quality) {
		int v;
		if (parse_int(quality, &v) && v >= 1 && v <= 100 && WebPConfigPreset(&m->config, WEBP_PRESET_DEFAULT, (float)v)) {
			m->quality = v;
		}
	}
	const char* max_length = parse_ctx->body->get_value(parse_ctx->cn, "max");
	if (max_length) {
		int parsed_max;
		if (parse_int(max_length, &parsed_max) && parsed_max > 0) {
			m->max_length = parsed_max;
		}
	}

	return KGL_OK;
}
static uint32_t process(KREQUEST rq, kgl_access_context* ctx, DWORD notify) {
	webp_mark* m = (webp_mark*)ctx->module;
	char buf[512];
	buf[0] = '\0';
	DWORD len = sizeof(buf);
	webp_context* c = (webp_context*)malloc(sizeof(webp_context));
	if (c == NULL) {
		return KF_STATUS_REQ_FALSE;
	}
	if (!init_webp_context(c, &m->config)) {
		free(c);
		return KF_STATUS_REQ_FINISHED;
	}
	c->max_length = m->max_length;
	if (c->max_length <= 0) {
		c->max_length = MAX_WEBP_SIZE;
	}
	if (KGL_OK == ctx->f->get_variable(rq, KGL_VAR_QUERY_STRING, NULL, buf, &len)) {
		int q;
		if (get_query_int(buf, "_wpq", &q)) {
			if (q >= 100) {
				free(c);
				return KF_STATUS_REQ_FALSE;
			}
			if (q >= 1) {
				WebPConfigPreset(&c->config, WEBP_PRESET_DEFAULT, (float)q);
			}
		}
	}
	len = sizeof(buf);
	buf[0] = '\0';
	if (KGL_OK == ctx->f->get_variable(rq, KGL_VAR_HEADER, "Accept", buf, &len) && is_support_webp(buf, len)) {
		c->accept_support = 1;
	}
	if (!register_upstream(rq, ctx, c)) {
		free(c);
		return KF_STATUS_REQ_FALSE;
	}
	return KF_STATUS_REQ_TRUE;
}
static kgl_access access_model = {
	sizeof(kgl_access),
	KF_NOTIFY_REQUEST_MARK,
	"webp",
	create_ctx,
	free_ctx,
	build,
	parse,
	NULL,
	process
};
void register_access(kgl_dso_version* ver) {
	KGL_REGISTER_ACCESS(ver, &access_model);
}
