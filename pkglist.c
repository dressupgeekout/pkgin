/*
 * Copyright (c) 2009-2015 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Emile "iMil" Heitor <imil@NetBSD.org> .
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "pkgin.h"
#include <regex.h>

#define PKG_EQUAL	'='
#define PKG_GREATER	'>'
#define PKG_LESSER	'<'

Plisthead	r_plisthead, l_plisthead;
int		r_plistcounter, l_plistcounter;

static void setfmt(char *, char *);

/*
 * Small structure for sorting package results.
 */
struct pkg_sort {
	char *full;
	char *name;
	char *version;
	char *comment;
	char flag;
};

/**
 * \fn malloc_pkglist
 *
 * \brief Pkglist allocation for all types of lists
 */
Pkglist *
malloc_pkglist(void)
{
	Pkglist *pkglist;

	pkglist = xmalloc(sizeof(Pkglist));

	/*!< Init all the things! (http://knowyourmeme.com/memes/x-all-the-y) */
	pkglist->full = NULL;
	pkglist->name = NULL;
	pkglist->version = NULL;
	pkglist->build_date = NULL;
	pkglist->depend = NULL;
	pkglist->size_pkg = 0;
	pkglist->old_size_pkg = -1;
	pkglist->file_size = 0;
	pkglist->level = 0;
	pkglist->download = 0;
	pkglist->pkgurl = NULL;
	pkglist->comment = NULL;
	pkglist->category = NULL;
	pkglist->pkgpath = NULL;
	pkglist->keep = 0;
	pkglist->action = DONOTHING;
	pkglist->old = NULL;

	return pkglist;
}

/**
 * \fn free_pkglist_entry
 *
 * \brief free a Pkglist single entry
 */
void
free_pkglist_entry(Pkglist **plist)
{
	XFREE((*plist)->full);
	XFREE((*plist)->name);
	XFREE((*plist)->version);
	XFREE((*plist)->depend);
	XFREE((*plist)->comment);
	XFREE((*plist)->category);
	XFREE((*plist)->pkgpath);
	XFREE((*plist)->old);
	XFREE(*plist);

	plist = NULL;
}

/**
 * \fn free_pkglist
 *
 * \brief Free all types of package list
 */
void
free_pkglist(Plisthead **plisthead)
{
	Pkglist *plist;

	if (*plisthead == NULL)
		return;

	while (!SLIST_EMPTY(*plisthead)) {
		plist = SLIST_FIRST(*plisthead);
		SLIST_REMOVE_HEAD(*plisthead, next);

		free_pkglist_entry(&plist);
	}
	XFREE(*plisthead);

	plisthead = NULL;
}

void
init_global_pkglists(void)
{
	Plistnumbered plist;

	SLIST_INIT(&r_plisthead);
	plist.P_Plisthead = &r_plisthead;
	plist.P_count = 0;
	plist.P_type = 1;
	pkgindb_doquery(REMOTE_PKGS_QUERY_ASC, pdb_rec_list, &plist);
	r_plistcounter = plist.P_count;

	SLIST_INIT(&l_plisthead);
	plist.P_Plisthead = &l_plisthead;
	plist.P_count = 0;
	plist.P_type = 0;
	pkgindb_doquery(LOCAL_PKGS_QUERY_ASC, pdb_rec_list, &plist);
	l_plistcounter = plist.P_count;
}

static void
free_global_pkglist(Plisthead *plisthead)
{
	Pkglist *plist;

	while (!SLIST_EMPTY(plisthead)) {
		plist = SLIST_FIRST(plisthead);
		SLIST_REMOVE_HEAD(plisthead, next);

		free_pkglist_entry(&plist);
	}
}

void
free_global_pkglists(void)
{
	free_global_pkglist(&l_plisthead);
	free_global_pkglist(&r_plisthead);
}

/**
 * \fn init_head
 *
 * \brief Init a Plisthead
 */
Plisthead *
init_head(void)
{
	Plisthead *plisthead;

	plisthead = xmalloc(sizeof(Plisthead));
	SLIST_INIT(plisthead);

	return plisthead;
}

/**
 * \fn rec_pkglist
 *
 * Record package list to SLIST
 */
Plistnumbered *
rec_pkglist(const char *fmt, ...)
{
	char		query[BUFSIZ];
	va_list		ap;
	Plistnumbered	*plist;

	plist = (Plistnumbered *)malloc(sizeof(Plistnumbered));
	plist->P_Plisthead = init_head();
	plist->P_count = 0;
	plist->P_type = 0;

	va_start(ap, fmt);
	vsnprintf(query, BUFSIZ, fmt, ap);
	va_end(ap);

	if (pkgindb_doquery(query, pdb_rec_list, plist) == PDB_OK)
		return plist;

	XFREE(plist->P_Plisthead);
	XFREE(plist);

	return NULL;
}

/* compare pkg version */
int
pkg_is_installed(Plisthead *plisthead, Pkglist *pkg)
{
	Pkglist *pkglist;

	SLIST_FOREACH(pkglist, plisthead, next) {
		/* make sure packages match */
		if (strcmp(pkglist->name, pkg->name) != 0)
			continue;

		/* exact same version */
		if (strcmp(pkglist->version, pkg->version) == 0)
			return 0;

		return version_check(pkglist->full, pkg->full);
	}

	return -1;
}

static void
setfmt(char *sfmt, char *pfmt)
{
	if (pflag) {
		strncpy(sfmt, "%s;%c", 6); /* snprintf(outpkg) */
		strncpy(pfmt, "%s;%s\n", 7); /* final printf */
	} else {
		strncpy(sfmt, "%s %c", 6);
		strncpy(pfmt, "%-20s %s\n", 10);
	}
}

void
list_pkgs(const char *pkgquery, int lstype)
{
	Pkglist	   	*plist;
	Plistnumbered	*plisthead;
	int		rc;
	char		pkgstatus, outpkg[BUFSIZ];
	char		sfmt[10], pfmt[10];

	setfmt(&sfmt[0], &pfmt[0]);

	/* list installed packages + status */
	if (lstype == PKG_LLIST_CMD && lslimit != '\0') {

		/* check if local package list is empty */
		if (SLIST_EMPTY(&l_plisthead)) {
			fprintf(stderr, MSG_EMPTY_LOCAL_PKGLIST);
			return;
		}

		if (!SLIST_EMPTY(&r_plisthead)) {

			SLIST_FOREACH(plist, &r_plisthead, next) {
				rc = pkg_is_installed(&l_plisthead, plist);

				pkgstatus = '\0';

				if (lslimit == PKG_EQUAL && rc == 0)
					pkgstatus = PKG_EQUAL;
				if (lslimit == PKG_GREATER && rc == 1)
					pkgstatus = PKG_GREATER;
				if (lslimit == PKG_LESSER && rc == 2)
					pkgstatus = PKG_LESSER;

				if (pkgstatus != '\0') {
					snprintf(outpkg, BUFSIZ, sfmt,
						plist->full, pkgstatus);
					printf(pfmt, outpkg, plist->comment);
				}

			}
		}

		return;
	} /* lstype == LLIST && status */

	/* regular package listing */
	if ((plisthead = rec_pkglist(pkgquery)) == NULL) {
		fprintf(stderr, MSG_EMPTY_LIST);
		return;
	}

	SLIST_FOREACH(plist, plisthead->P_Plisthead, next)
		printf(pfmt, plist->full, plist->comment);

	free_pkglist(&plisthead->P_Plisthead);
	free(plisthead);
}

/*
 * Sort a list of packages first by package name (alphabetically) and then by
 * version (highest first).
 */
static int
pkg_sort_cmp(const void *a, const void *b)
{
	const struct pkg_sort p1 = *(const struct pkg_sort *)a;
	const struct pkg_sort p2 = *(const struct pkg_sort *)b;

	/*
	 * First compare name, if they are the same then fall through to
	 * a version comparison.
	 */
	if (strcmp(p1.name, p2.name) > 0)
		return 1;
	else if (strcmp(p1.name, p2.name) < 0)
		return -1;

	if (dewey_cmp(p1.version, DEWEY_LT, p2.version))
		return 1;
	else if (dewey_cmp(p1.version, DEWEY_GT, p2.version))
		return -1;

	return 0;
}

int
search_pkg(const char *pattern)
{
	Pkglist	   	*plist;
	struct pkg_sort	*psort;
	regex_t		re;
	int		i, rc;
	char		eb[64], is_inst, outpkg[BUFSIZ];
	int		matched = 0, pcount = 0;
	char		sfmt[10], pfmt[10];

	setfmt(&sfmt[0], &pfmt[0]);

	if ((rc = regcomp(&re, pattern,
	    REG_EXTENDED|REG_NOSUB|REG_ICASE)) != 0) {
		regerror(rc, &re, eb, sizeof(eb));
		errx(EXIT_FAILURE, "regcomp: %s: %s", pattern, eb);
	}

	psort = xmalloc(sizeof(struct pkg_sort));

	SLIST_FOREACH(plist, &r_plisthead, next) {
		if (regexec(&re, plist->name, 0, NULL, 0) == 0 ||
		    regexec(&re, plist->full, 0, NULL, 0) == 0 ||
		    regexec(&re, plist->comment, 0, NULL, 0) == 0) {
			matched = 1;
			rc = pkg_is_installed(&l_plisthead, plist);

			if (rc == 0)
				is_inst = PKG_EQUAL;
			else if (rc == 1)
				is_inst = PKG_GREATER;
			else if (rc == 2)
				is_inst = PKG_LESSER;
			else
				is_inst = '\0';

			psort = xrealloc(psort, (pcount + 1) * sizeof(*psort));
			psort[pcount].full = xstrdup(plist->full);
			psort[pcount].name = xstrdup(plist->name);
			psort[pcount].version = xstrdup(plist->version);
			psort[pcount].comment = xstrdup(plist->comment);
			psort[pcount++].flag = is_inst;
		}
	}

	qsort(psort, pcount, sizeof(struct pkg_sort), pkg_sort_cmp);

	for (i = 0; i < pcount; i++) {
		snprintf(outpkg, BUFSIZ, sfmt, psort[i].full, psort[i].flag);
		printf(pfmt, outpkg, psort[i].comment);

		XFREE(psort[i].name);
		XFREE(psort[i].comment);
	}

	XFREE(psort);

	regfree(&re);

	if (matched) {
		printf(MSG_IS_INSTALLED_CODE);
		return EXIT_SUCCESS;
	} else {
		printf(MSG_NO_SEARCH_RESULTS, pattern);
		return EXIT_FAILURE;
	}
}

void
show_category(char *category)
{
	Pkglist	   	*plist;

	SLIST_FOREACH(plist, &r_plisthead, next) {
		if (plist->category == NULL)
			continue;
		if (strcmp(plist->category, category) == 0)
			printf("%-20s %s\n", plist->full, plist->comment);
	}
}

int
show_pkg_category(char *pkgname)
{
	Pkglist	   	*plist;
	int		matched = 0;

	SLIST_FOREACH(plist, &r_plisthead, next) {
		if (strcmp(plist->name, pkgname) == 0) {
			matched = 1;
			if (plist->category == NULL)
				continue;
			printf("%-12s - %s\n", plist->category, plist->full);
		}
	}

	if (matched)
		return EXIT_SUCCESS;
	else {
		fprintf(stderr, MSG_PKG_NOT_AVAIL, pkgname);
		return EXIT_FAILURE;
	}
}

void
show_all_categories(void)
{
	Plistnumbered	*cathead;
	Pkglist			*plist;

	if ((cathead = rec_pkglist(SHOW_ALL_CATEGORIES)) == NULL) {
		fprintf(stderr, MSG_NO_CATEGORIES);
		return;
	}

	SLIST_FOREACH(plist, cathead->P_Plisthead, next)
		printf("%s\n", plist->full);

	free_pkglist(&cathead->P_Plisthead);
	free(cathead);
}
