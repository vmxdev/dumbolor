#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <wchar.h>
#include <wctype.h>
#include <locale.h>
#include <time.h>

#include <libircclient.h>

#define MESSAGE_LEN (512*2)
#define NICK_LEN 32

#define CONCAT(A, B)  A ## B
#define PREFIX_L(A) CONCAT(L, A)

#define NICK "tsar1488"
#define NICK_WIDE PREFIX_L(NICK)

struct dict_entry
{
	char txt[512];
};

struct irc_ctx
{
	char *channel;
	char *nick;
	char last_msg[MESSAGE_LEN];

	size_t dict_size;
	struct dict_entry *dict;
};

static int
load_dict(struct irc_ctx *ctx)
{
	FILE *f;
	char *line = NULL;
	size_t len = 0;
	ssize_t nread;

	f = fopen("tsar.txt", "r");
	if (!f) {
		return 0;
	}

	free(ctx->dict);
	ctx->dict_size = 0;
	ctx->dict = NULL;

	while ((nread = getline(&line, &len, f) != -1)) {
		ctx->dict = realloc(ctx->dict,
			sizeof(struct dict_entry) * (ctx->dict_size + 1));
		strcpy(ctx->dict[ctx->dict_size].txt, line);
		ctx->dict_size++;
	}

	fclose(f);
	return 1;
}

static int
mk_response(struct irc_ctx *ctx, char *nick, char *message)
{
	size_t idx;
	size_t msgsize;
	wchar_t wmsg[MESSAGE_LEN], wmsg_l[MESSAGE_LEN];
	size_t i;
	int react;

	/* convert message to array of wchat_t */
	msgsize = mbstowcs(wmsg, message, MESSAGE_LEN);
	for (i=0; i<msgsize; i++) {
		/* and make it lowercase */
		wmsg_l[i] = towlower(wmsg[i]);
	}
	wmsg_l[msgsize + 1] = L'\0';

	react = (wcsstr(wmsg_l, L"цар") || wcsstr(wmsg_l, NICK_WIDE));
	if (!react) {
		return 0;
	}

	if (!load_dict(ctx)) {
		return 0;
	}

	idx = (size_t)((double)rand() / ((double)RAND_MAX + 1) * ctx->dict_size);

	strcpy(message, ctx->dict[idx].txt);
	strcpy(ctx->last_msg, message);
	return 1;
}

static void
event_connect(irc_session_t *session, const char *event, const char *origin,
		const char **params, unsigned int count)
{
	struct irc_ctx *ctx;
	(void)event;
	(void)origin;
	(void)params;
	(void)count;

	ctx = (struct irc_ctx *)irc_get_ctx(session);
	irc_cmd_join(session, ctx->channel, 0);
}

static void
event_channel(irc_session_t *session, const char *event, const char *origin,
		const char **params, unsigned int count)
{
	struct irc_ctx *ctx;
	char nick[NICK_LEN];
	char message[MESSAGE_LEN], resp[MESSAGE_LEN];
	(void)event;
	(void)count;

	ctx = (struct irc_ctx *)irc_get_ctx(session);

	irc_target_get_nick(origin, nick, sizeof(nick));
	strcpy(message, params[1]);

	if (mk_response(ctx, nick, message)) {
		sprintf(resp, "%s: %s", nick, message);
		irc_cmd_msg(session, params[0], resp);
	}
}

static void
event_ctcp_request(irc_session_t *session, const char *event, const char *origin,
		const char **params, unsigned int count)
{
	(void)event;
	(void)count;

	if (origin) {
		char nickbuf[128], textbuf[256];
		irc_target_get_nick (origin, nickbuf, sizeof(nickbuf));

		if (strstr (params[0], "PING") == params[0])
			irc_cmd_ctcp_reply (session, nickbuf, params[0]);
		else if ( !strcmp (params[0], "VERSION") ) {
/*
			unsigned int high, low;
			irc_get_version (&high, &low);
*/
			sprintf (textbuf, "VERSION Лалки сасатб");
			irc_cmd_ctcp_reply (session, nickbuf, textbuf);
		} else if ( !strcmp (params[0], "FINGER") ) {
			sprintf (textbuf, "FINGER %s (%s) Idle 0 seconds", 
				/*session->username ? session->username :*/ "nobody",
				/*session->realname ? session->realname :*/ "noname");

			irc_cmd_ctcp_reply (session, nickbuf, textbuf);
		} else if ( !strcmp (params[0], "TIME") ) {
			time_t now = time(0);
			struct tm tmtmp, *ltime = localtime_r (&now, &tmtmp);

			strftime (textbuf, sizeof(textbuf), "%a %b %d %H:%M:%S %Z %Y", ltime);
			irc_cmd_ctcp_reply (session, nickbuf, textbuf);
		}
	}
}


int
main()
{
	irc_callbacks_t callbacks;
	irc_session_t *session;
	struct irc_ctx ctx;
	unsigned short port = 6667;
	char *server = "irc.freenode.net";

	setlocale(LC_ALL, "");

	memset(&callbacks, 0, sizeof(callbacks));
	callbacks.event_connect = event_connect;
	callbacks.event_channel = event_channel;
	callbacks.event_ctcp_req = event_ctcp_request;

	session = irc_create_session(&callbacks);
	if (!session) {
		fprintf(stderr, "Can't create session\n");
		return EXIT_FAILURE;
	}

	ctx.channel = "#chlor";
	ctx.nick = NICK;
	ctx.last_msg[0] = '\0';
	ctx.dict = NULL;
	irc_set_ctx(session, &ctx);

	if (irc_connect(session, server, port, 0, ctx.nick, 0, 0)) {
		fprintf(stderr, "Can't connect: %s\n",
			irc_strerror(irc_errno(session)));
		return EXIT_FAILURE;
	}
	if (irc_run(session)) {
		fprintf(stderr, "Error: %s\n",
			irc_strerror(irc_errno(session)));
		return EXIT_FAILURE;
	}


	return EXIT_SUCCESS;
}

