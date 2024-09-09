#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* For a demo (and actually for most cases) it's more than enough */
#define MAXRULECOUNT 128

typedef struct Rule Rule;

struct Rule {
	unsigned count;
	enum {
		RuleBytes,
		RuleAlt,
		RuleSeq
	} type;
	union {
		char bytes[MAXRULECOUNT];
		Rule *rules[MAXRULECOUNT];
	} as;
};

#define BYTES(r, s) ({\
	char bytes[] = s;\
	(r)->count = sizeof(bytes) - 1;\
	strncpy((r)->as.bytes, s, (r)->count);\
	(r)->type = RuleBytes;\
})

#define ALT(r, args...) ({\
	Rule *rules[] = {args};\
	(r)->count = sizeof(rules)/sizeof(rules[0]);\
	memcpy((r)->as.rules, rules, sizeof(rules));\
	(r)->type = RuleAlt;\
})

#define SEQ(r, args...) ({\
	Rule *rules[] = {args};\
	(r)->count = sizeof(rules)/sizeof(rules[0]);\
	memcpy((r)->as.rules, rules, sizeof(rules));\
	(r)->type = RuleSeq;\
})


typedef struct State State;

struct State {
	Rule     *r;
	unsigned i;
	unsigned start;
	State    *childs[MAXRULECOUNT];
	State    *parent;
};

State *state(Rule *r, unsigned start, State *parent)
{
	State *s = malloc(sizeof(*s));
	memset(s, 0, sizeof(*s));
	s->r = r;
	s->start = start;
	s->parent = parent;
	return s;
}

void freestate(State *s)
{
	unsigned i;
	if (!s)
		return;
	for (i = 0; i < s->r->count; i++)
		freestate(s->childs[i]);
	free(s);
}

typedef struct {
	State    *pos;
	char     *input;
	unsigned cap;
	unsigned count;
	unsigned next;
} Parser;

void pushchar(Parser *p, char c)
{
	if (p->count + 1 > p->cap) {
		p->cap = p->cap*2 + 1;
		p->input = realloc(p->input, p->cap);
	}
	p->input[p->count] = c;
	p->count += 1;
}

void pushrule(Parser *p, Rule *r)
{
	for (;;) {
		State *s = state(r, p->next, p->pos);
		if (p->pos)
			p->pos->childs[p->pos->i] = s;
		p->pos = s;
		if (r->type == RuleBytes)
			break;
		r = r->as.rules[0];
	}
}

Parser *parser(Rule *root)
{
	Parser *p = malloc(sizeof(*p));
	memset(p, 0, sizeof(*p));
	pushrule(p, root);
	return p;
}

void freeparser(Parser *p)
{
	if (p->pos) {
		while (p->pos->parent)
			p->pos = p->pos->parent;
		freestate(p->pos);
	}
	free(p->input);
	free(p);
}

void advance(Parser *p)
{
	while (p->pos) {
		State *s = p->pos;
		if (s->r->type == RuleSeq && s->i+1 < s->r->count) {
			s->i += 1;
			pushrule(p, s->r->as.rules[s->i]);
			return;
		}
		p->pos = s->parent;
		if (!s->parent)
			freestate(s); /* we are done, don't leak the pointer */
	}
}

void backtrack(Parser *p)
{
	while (p->pos) {
		State *s = p->pos;
		s->childs[s->i] = 0; /* at this point the child is either null or was freed */
		if (s->r->type == RuleAlt && s->i+1 < s->r->count) {
			s->i += 1;
			p->next = s->start;
			pushrule(p, s->r->as.rules[s->i]);
			return;
		}
		if (s->r->type == RuleSeq && s->i > 0) {
			s->i -= 1;
			while (s->r->type != RuleBytes)
				s = s->childs[s->i];
			p->pos = s;
		} else {
			p->pos = s->parent;
			freestate(s);
		}
	}
}

void step(Parser *p)
{
	State *s = p->pos;
	if (!s)
		return;
	if (s->r->as.bytes[s->i] != p->input[p->next]) {
		backtrack(p);
		return;
	}
	s->i += 1;
	p->next += 1;
	if (s->i == s->r->count)
		advance(p);
}

int done(Parser *p)
{
	return !p->pos;
}

int succeeded(Parser *p)
{
	return !p->pos && p->next == p->count;
}

void feed(Parser *p, char c)
{
	pushchar(p, c);
	while (p->next < p->count) {
		if (done(p))
			return;
		step(p);
	}
}

void stop(Parser *p)
{
	while (!done(p)) {
		backtrack(p);
		while (p->next < p->count && !done(p))
			step(p);
	}
}


Rule *aaaab(void)
{
	/* aaaab = "ab" | "a" aaaab */
	static Rule a, ab, aab, aaaab;
	BYTES(&a, "a");
	BYTES(&ab, "ab");
	SEQ(&aab, &a, &aaaab);
	ALT(&aaaab, &ab, &aab);
	return &aaaab;
}

Rule *abcd(void)
{
	/* abcd = ("a" | "ab") ("bd" | "c") */
	static Rule a, ab, aab, bd, c, bdc, abcd;
	BYTES(&a, "a");
	BYTES(&ab, "ab");
	BYTES(&bd, "bd");
	BYTES(&c, "c");
	ALT(&aab, &a, &ab);
	ALT(&bdc, &bd, &c);
	SEQ(&abcd, &aab, &bdc);
	return &abcd;
}

Rule *number(void)
{
	static Rule v1, v2, v3, v4, v5, v6, v7, v8, v9, v0;
	static Rule digit, digits, number;
	BYTES(&v1, "1"); BYTES(&v2, "2");
	BYTES(&v3, "3"); BYTES(&v4, "4");
	BYTES(&v5, "5"); BYTES(&v6, "6");
	BYTES(&v7, "7"); BYTES(&v8, "8");
	BYTES(&v9, "9"); BYTES(&v0, "0");
	ALT(&digit, &v1, &v2, &v3, &v4, &v5, &v6, &v7, &v8, &v9, &v0);
	SEQ(&digits, &digit, &number);
	ALT(&number, &digits, &digit);
	return &number;
}

Rule *treeof(Rule *item)
{
	/* tree = item | "{" tree "," tree "}" */
	static Rule coma, lp, rp, node, tree;
	BYTES(&coma, ",");
	BYTES(&lp, "{");
	BYTES(&rp, "}");
	SEQ(&node, &lp, &tree, &coma, &tree, &rp);
	ALT(&tree, item, &node);
	return &tree;
}

Rule *sepby(Rule *item, Rule *sep)
{
	/* sepby = item | item sep sepby */
	static Rule rep, seq;
	SEQ(&rep, item, sep, &seq);
	ALT(&seq, &rep, item);
	return &seq;
}

Rule *spaces(void)
{
	/* spaces = ws | ws spaces, ws = " " | "\t" */
	static Rule sp, tab, ws, wsseq, spaces;
	BYTES(&sp, " ");
	BYTES(&tab, "\t");
	ALT(&ws, &sp, &tab);
	SEQ(&wsseq, &ws, &spaces);
	ALT(&spaces, &wsseq, &ws);
	return &spaces;
}

void parse(Rule *r)
{
	Parser *p = parser(r);
	while (!done(p) || succeeded(p)) {
		int c = getchar();
		if (c == EOF) {
			stop(p);
			break;
		}
		if (c != '\n')
			feed(p, c);
	}
	if (succeeded(p))
		printf("OK\n");
	else
		printf("FAIL\n");
	freeparser(p);
}


int main(void)
{
	parse(sepby(number(), spaces()));
	return 0;
}
