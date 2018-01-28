#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <libircclient.h>

#define MESSAGE_LEN 512
#define NICK_LEN 32

struct irc_ctx
{
	char *channel;
	char *nick;
	char last_msg[MESSAGE_LEN];
	int nmessages;
	char *messages;
};

/* messages comparator */
static int
msg_cmp(const void *p1, const void *p2)
{
	const char *s1 = p1;
	const char *s2 = p2;
	return strcmp(s1, s2);
}

/* construct response */
static int
make_message(struct irc_ctx *ctx, char *nick, char *message)
{
	int idx, i;
	char *raw, *tok_begin, *tok_end;
	int ntokens = 0;
	char tokens[MESSAGE_LEN][MESSAGE_LEN];

	/* take random message */
	idx = rand() % ctx->nmessages;
	raw = &ctx->messages[idx * MESSAGE_LEN];

	/* tokenize text */
	tok_begin = tok_end = raw;
	while (*tok_end) {
		char c;

		c = *tok_end;
		if (isspace(c) || (ispunct(c))) {
			if (tok_begin != tok_end) {
				memcpy(tokens[ntokens], tok_begin, tok_end - tok_begin);
				tokens[ntokens][tok_end - tok_begin] = '\0';
				if (strcmp(tokens[ntokens], ctx->nick) != 0) {
					/* skip own nickname */
					ntokens++;
				}
			}
			tok_end++;
			tok_begin = tok_end;
		} else {
			tok_end++;
		}
	}
	if (tok_begin != tok_end) {
		memcpy(tokens[ntokens], tok_begin, tok_end - tok_begin);
		tokens[ntokens][tok_end - tok_begin] = '\0';
		if (strcmp(tokens[ntokens], ctx->nick) != 0) {
			ntokens++;
		}
	}

	if (ntokens == 0) {
		return 0;
	}

	/* shuffle */
	for (i=0; i<2; i++) {
		int idx1, idx2;
		char buf[MESSAGE_LEN];

		idx1 = rand() % ntokens;
		idx2 = rand() % ntokens;
		if (idx1 == idx2) {
			continue;
		}
		/* swap tokens */
		strcpy(buf, tokens[idx1]);
		strcpy(tokens[idx1], tokens[idx2]);
		strcpy(tokens[idx2], buf);
	}

	/* concatenate tokens */
	sprintf(message, "%s: ", nick);
	for (i=0; i<ntokens; i++) {
		strcat(message, tokens[i]);
		strcat(message, " ");
	}

	if (strcmp(ctx->last_msg, message) == 0) {
		/* don't send the same mesage twice */
		return 0;
	}
	/* save last message */
	strcpy(ctx->last_msg, message);
	return 1;
}

/* store message */
static void
add_message(struct irc_ctx *ctx, const char *message)
{
	char *tmp_msgs;

	if (ctx->nmessages > 0) {
		void *msg;

		msg = bsearch(message, ctx->messages, ctx->nmessages, MESSAGE_LEN, &msg_cmp);
		if (msg) {
			/* message already exists */
			return;
		}
	}

	/* append message to tail */
	tmp_msgs = realloc(ctx->messages, (ctx->nmessages + 1) * MESSAGE_LEN);
	if (!tmp_msgs) {
		fprintf(stderr, "realloc() failed\n");
		exit(1);
	}
	ctx->messages = tmp_msgs;
	strcpy(&ctx->messages[ctx->nmessages * MESSAGE_LEN], message);
	ctx->nmessages++;
	/* sort messages */
	qsort(ctx->messages, ctx->nmessages, MESSAGE_LEN, &msg_cmp);
}

static void
event_connect(irc_session_t *session, const char *event, const char *origin,
		const char **params, unsigned int count)
{
	(void)event;
	(void)origin;
	(void)params;
	(void)count;
	struct irc_ctx *ctx;

	ctx = (struct irc_ctx *)irc_get_ctx(session);
	irc_cmd_join(session, ctx->channel, 0);
}

static void
event_channel(irc_session_t *session, const char *event, const char *origin,
		const char **params, unsigned int count)
{
	(void)event;
	(void)count;
	struct irc_ctx *ctx;
	char nick[NICK_LEN];
	char message[MESSAGE_LEN];

	ctx = (struct irc_ctx *)irc_get_ctx(session);
	add_message(ctx, params[1]);
	/* for debug */
	printf("%s\n", params[1]);

	if (strstr(params[1], ctx->nick) != params[1]) {
		return;
	}
	irc_target_get_nick(origin, nick, sizeof(nick));

	if (ctx->nmessages > 1) {
		if (make_message(ctx, nick, message)) {
			irc_cmd_msg(session, params[0], message);
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

	memset(&callbacks, 0, sizeof(callbacks));
	callbacks.event_connect = event_connect;
	callbacks.event_channel = event_channel;

	session = irc_create_session(&callbacks);
	if (!session) {
		fprintf(stderr, "Can't create session\n");
		return EXIT_FAILURE;
	}

	ctx.channel = "#lor";
	ctx.nick = "bot1488";
	ctx.last_msg[0] = '\0';
	ctx.nmessages = 0;
	ctx.messages = NULL;
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

