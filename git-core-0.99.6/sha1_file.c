/*
 * GIT - The information manager from hell
 *
 * Copyright (C) Linus Torvalds, 2005
 *
 * This handles basic git sha1 object files - packing, unpacking,
 * creation etc.
 */
#include <sys/types.h>
#include <dirent.h>
#include "cache.h"
#include "delta.h"
#include "pack.h"

#ifndef O_NOATIME
#if defined(__linux__) && (defined(__i386__) || defined(__PPC__))
#define O_NOATIME 01000000
#else
#define O_NOATIME 0
#endif
#endif

static unsigned int sha1_file_open_flag = O_NOATIME;

static unsigned hexval(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return ~0;
}

int get_sha1_hex(const char *hex, unsigned char *sha1)
{
	int i;
	for (i = 0; i < 20; i++) {
		unsigned int val = (hexval(hex[0]) << 4) | hexval(hex[1]);
		if (val & ~0xff)
			return -1;
		*sha1++ = val;
		hex += 2;
	}
	return 0;
}

static char *git_dir, *git_object_dir, *git_index_file, *git_refs_dir,
	*git_graft_file;
static void setup_git_env(void)
{
	git_dir = gitenv(GIT_DIR_ENVIRONMENT);
	if (!git_dir)
		git_dir = DEFAULT_GIT_DIR_ENVIRONMENT;
	git_object_dir = gitenv(DB_ENVIRONMENT);
	if (!git_object_dir) {
		git_object_dir = xmalloc(strlen(git_dir) + 9);
		sprintf(git_object_dir, "%s/objects", git_dir);
	}
	git_refs_dir = xmalloc(strlen(git_dir) + 6);
	sprintf(git_refs_dir, "%s/refs", git_dir);
	git_index_file = gitenv(INDEX_ENVIRONMENT);
	if (!git_index_file) {
		git_index_file = xmalloc(strlen(git_dir) + 7);
		sprintf(git_index_file, "%s/index", git_dir);
	}
	git_graft_file = gitenv(GRAFT_ENVIRONMENT);
	if (!git_graft_file)
		git_graft_file = strdup(git_path("info/grafts"));
}

char *get_object_directory(void)
{
	if (!git_object_dir)
		setup_git_env();
	return git_object_dir;
}

char *get_refs_directory(void)
{
	if (!git_refs_dir)
		setup_git_env();
	return git_refs_dir;
}

char *get_index_file(void)
{
	if (!git_index_file)
		setup_git_env();
	return git_index_file;
}

char *get_graft_file(void)
{
	if (!git_graft_file)
		setup_git_env();
	return git_graft_file;
}

int safe_create_leading_directories(char *path)
{
	char *pos = path;

	while (pos) {
		pos = strchr(pos, '/');
		if (!pos)
			break;
		*pos = 0;
		if (mkdir(path, 0777) < 0)
			if (errno != EEXIST) {
				*pos = '/';
				return -1;
			}
		*pos++ = '/';
	}
	return 0;
}

char * sha1_to_hex(const unsigned char *sha1)
{
	static char buffer[50];
	static const char hex[] = "0123456789abcdef";
	char *buf = buffer;
	int i;

	for (i = 0; i < 20; i++) {
		unsigned int val = *sha1++;
		*buf++ = hex[val >> 4];
		*buf++ = hex[val & 0xf];
	}
	return buffer;
}

static void fill_sha1_path(char *pathbuf, const unsigned char *sha1)
{
	int i;
	for (i = 0; i < 20; i++) {
		static char hex[] = "0123456789abcdef";
		unsigned int val = sha1[i];
		char *pos = pathbuf + i*2 + (i > 0);
		*pos++ = hex[val >> 4];
		*pos = hex[val & 0xf];
	}
}

/*
 * NOTE! This returns a statically allocated buffer, so you have to be
 * careful about using it. Do a "strdup()" if you need to save the
 * filename.
 *
 * Also note that this returns the location for creating.  Reading
 * SHA1 file can happen from any alternate directory listed in the
 * DB_ENVIRONMENT environment variable if it is not found in
 * the primary object database.
 */
char *sha1_file_name(const unsigned char *sha1)
{
	static char *name, *base;

	if (!base) {
		const char *sha1_file_directory = get_object_directory();
		int len = strlen(sha1_file_directory);
		base = xmalloc(len + 60);
		memcpy(base, sha1_file_directory, len);
		memset(base+len, 0, 60);
		base[len] = '/';
		base[len+3] = '/';
		name = base + len + 1;
	}
	fill_sha1_path(name, sha1);
	return base;
}

char *sha1_pack_name(const unsigned char *sha1)
{
	static const char hex[] = "0123456789abcdef";
	static char *name, *base, *buf;
	int i;

	if (!base) {
		const char *sha1_file_directory = get_object_directory();
		int len = strlen(sha1_file_directory);
		base = xmalloc(len + 60);
		sprintf(base, "%s/pack/pack-1234567890123456789012345678901234567890.pack", sha1_file_directory);
		name = base + len + 11;
	}

	buf = name;

	for (i = 0; i < 20; i++) {
		unsigned int val = *sha1++;
		*buf++ = hex[val >> 4];
		*buf++ = hex[val & 0xf];
	}
	
	return base;
}

char *sha1_pack_index_name(const unsigned char *sha1)
{
	static const char hex[] = "0123456789abcdef";
	static char *name, *base, *buf;
	int i;

	if (!base) {
		const char *sha1_file_directory = get_object_directory();
		int len = strlen(sha1_file_directory);
		base = xmalloc(len + 60);
		sprintf(base, "%s/pack/pack-1234567890123456789012345678901234567890.idx", sha1_file_directory);
		name = base + len + 11;
	}

	buf = name;

	for (i = 0; i < 20; i++) {
		unsigned int val = *sha1++;
		*buf++ = hex[val >> 4];
		*buf++ = hex[val & 0xf];
	}
	
	return base;
}

struct alternate_object_database *alt_odb_list;
static struct alternate_object_database **alt_odb_tail;

/*
 * Prepare alternate object database registry.
 *
 * The variable alt_odb_list points at the list of struct
 * alternate_object_database.  The elements on this list come from
 * non-empty elements from colon separated ALTERNATE_DB_ENVIRONMENT
 * environment variable, and $GIT_OBJECT_DIRECTORY/info/alternates,
 * whose contents is exactly in the same format as that environment
 * variable.  Its base points at a statically allocated buffer that
 * contains "/the/directory/corresponding/to/.git/objects/...", while
 * its name points just after the slash at the end of ".git/objects/"
 * in the example above, and has enough space to hold 40-byte hex
 * SHA1, an extra slash for the first level indirection, and the
 * terminating NUL.
 */
static void link_alt_odb_entries(const char *alt, const char *ep, int sep)
{
	const char *cp, *last;
	struct alternate_object_database *ent;

	last = alt;
	while (last < ep) {
		cp = last;
		if (cp < ep && *cp == '#') {
			while (cp < ep && *cp != sep)
				cp++;
			last = cp + 1;
			continue;
		}
		for ( ; cp < ep && *cp != sep; cp++)
			;
		if (last != cp) {
			/* 43 = 40-byte + 2 '/' + terminating NUL */
			int pfxlen = cp - last;
			int entlen = pfxlen + 43;

			ent = xmalloc(sizeof(*ent) + entlen);
			*alt_odb_tail = ent;
			alt_odb_tail = &(ent->next);
			ent->next = NULL;

			memcpy(ent->base, last, pfxlen);
			ent->name = ent->base + pfxlen + 1;
			ent->base[pfxlen] = ent->base[pfxlen + 3] = '/';
			ent->base[entlen-1] = 0;
		}
		while (cp < ep && *cp == sep)
			cp++;
		last = cp;
	}
}

void prepare_alt_odb(void)
{
	char path[PATH_MAX];
	char *map;
	int fd;
	struct stat st;
	char *alt;

	alt = gitenv(ALTERNATE_DB_ENVIRONMENT);
	if (!alt) alt = "";

	sprintf(path, "%s/info/alternates", get_object_directory());
	if (alt_odb_tail)
		return;
	alt_odb_tail = &alt_odb_list;
	link_alt_odb_entries(alt, alt + strlen(alt), ':');

	fd = open(path, O_RDONLY);
	if (fd < 0)
		return;
	if (fstat(fd, &st) || (st.st_size == 0)) {
		close(fd);
		return;
	}
	map = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	if (map == MAP_FAILED)
		return;

	link_alt_odb_entries(map, map + st.st_size, '\n');
	munmap(map, st.st_size);
}

static char *find_sha1_file(const unsigned char *sha1, struct stat *st)
{
	char *name = sha1_file_name(sha1);
	struct alternate_object_database *alt;

	if (!stat(name, st))
		return name;
	prepare_alt_odb();
	for (alt = alt_odb_list; alt; alt = alt->next) {
		name = alt->name;
		fill_sha1_path(name, sha1);
		if (!stat(alt->base, st))
			return alt->base;
	}
	return NULL;
}

#define PACK_MAX_SZ (1<<26)
static int pack_used_ctr;
static unsigned long pack_mapped;
struct packed_git *packed_git;

static int check_packed_git_idx(const char *path, unsigned long *idx_size_,
				void **idx_map_)
{
	void *idx_map;
	unsigned int *index;
	unsigned long idx_size;
	int nr, i;
	int fd = open(path, O_RDONLY);
	struct stat st;
	if (fd < 0)
		return -1;
	if (fstat(fd, &st)) {
		close(fd);
		return -1;
	}
	idx_size = st.st_size;
	idx_map = mmap(NULL, idx_size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	if (idx_map == MAP_FAILED)
		return -1;

	index = idx_map;
	*idx_map_ = idx_map;
	*idx_size_ = idx_size;

	/* check index map */
	if (idx_size < 4*256 + 20 + 20)
		return error("index file too small");
	nr = 0;
	for (i = 0; i < 256; i++) {
		unsigned int n = ntohl(index[i]);
		if (n < nr)
			return error("non-monotonic index");
		nr = n;
	}

	/*
	 * Total size:
	 *  - 256 index entries 4 bytes each
	 *  - 24-byte entries * nr (20-byte sha1 + 4-byte offset)
	 *  - 20-byte SHA1 of the packfile
	 *  - 20-byte SHA1 file checksum
	 */
	if (idx_size != 4*256 + nr * 24 + 20 + 20)
		return error("wrong index file size");

	return 0;
}

static int unuse_one_packed_git(void)
{
	struct packed_git *p, *lru = NULL;

	for (p = packed_git; p; p = p->next) {
		if (p->pack_use_cnt || !p->pack_base)
			continue;
		if (!lru || p->pack_last_used < lru->pack_last_used)
			lru = p;
	}
	if (!lru)
		return 0;
	munmap(lru->pack_base, lru->pack_size);
	lru->pack_base = NULL;
	return 1;
}

void unuse_packed_git(struct packed_git *p)
{
	p->pack_use_cnt--;
}

int use_packed_git(struct packed_git *p)
{
	if (!p->pack_size) {
		struct stat st;
		// We created the struct before we had the pack
		stat(p->pack_name, &st);
		if (!S_ISREG(st.st_mode))
			die("packfile %s not a regular file", p->pack_name);
		p->pack_size = st.st_size;
	}
	if (!p->pack_base) {
		int fd;
		struct stat st;
		void *map;

		pack_mapped += p->pack_size;
		while (PACK_MAX_SZ < pack_mapped && unuse_one_packed_git())
			; /* nothing */
		fd = open(p->pack_name, O_RDONLY);
		if (fd < 0)
			die("packfile %s cannot be opened", p->pack_name);
		if (fstat(fd, &st)) {
			close(fd);
			die("packfile %s cannot be opened", p->pack_name);
		}
		if (st.st_size != p->pack_size)
			die("packfile %s size mismatch.", p->pack_name);
		map = mmap(NULL, p->pack_size, PROT_READ, MAP_PRIVATE, fd, 0);
		close(fd);
		if (map == MAP_FAILED)
			die("packfile %s cannot be mapped.", p->pack_name);
		p->pack_base = map;

		/* Check if the pack file matches with the index file.
		 * this is cheap.
		 */
		if (memcmp((char*)(p->index_base) + p->index_size - 40,
			   p->pack_base + p->pack_size - 20, 20)) {
			      
			die("packfile %s does not match index.", p->pack_name);
		}
	}
	p->pack_last_used = pack_used_ctr++;
	p->pack_use_cnt++;
	return 0;
}

struct packed_git *add_packed_git(char *path, int path_len)
{
	struct stat st;
	struct packed_git *p;
	unsigned long idx_size;
	void *idx_map;

	if (check_packed_git_idx(path, &idx_size, &idx_map))
		return NULL;

	/* do we have a corresponding .pack file? */
	strcpy(path + path_len - 4, ".pack");
	if (stat(path, &st) || !S_ISREG(st.st_mode)) {
		munmap(idx_map, idx_size);
		return NULL;
	}
	/* ok, it looks sane as far as we can check without
	 * actually mapping the pack file.
	 */
	p = xmalloc(sizeof(*p) + path_len + 2);
	strcpy(p->pack_name, path);
	p->index_size = idx_size;
	p->pack_size = st.st_size;
	p->index_base = idx_map;
	p->next = NULL;
	p->pack_base = NULL;
	p->pack_last_used = 0;
	p->pack_use_cnt = 0;
	return p;
}

struct packed_git *parse_pack_index(unsigned char *sha1)
{
	char *path = sha1_pack_index_name(sha1);
	return parse_pack_index_file(sha1, path);
}

struct packed_git *parse_pack_index_file(const unsigned char *sha1, char *idx_path)
{
	struct packed_git *p;
	unsigned long idx_size;
	void *idx_map;
	char *path;

	if (check_packed_git_idx(idx_path, &idx_size, &idx_map))
		return NULL;

	path = sha1_pack_name(sha1);

	p = xmalloc(sizeof(*p) + strlen(path) + 2);
	strcpy(p->pack_name, path);
	p->index_size = idx_size;
	p->pack_size = 0;
	p->index_base = idx_map;
	p->next = NULL;
	p->pack_base = NULL;
	p->pack_last_used = 0;
	p->pack_use_cnt = 0;
	memcpy(p->sha1, sha1, 20);
	return p;
}

void install_packed_git(struct packed_git *pack)
{
	pack->next = packed_git;
	packed_git = pack;
}

static void prepare_packed_git_one(char *objdir)
{
	char path[PATH_MAX];
	int len;
	DIR *dir;
	struct dirent *de;

	sprintf(path, "%s/pack", objdir);
	len = strlen(path);
	dir = opendir(path);
	if (!dir)
		return;
	path[len++] = '/';
	while ((de = readdir(dir)) != NULL) {
		int namelen = strlen(de->d_name);
		struct packed_git *p;

		if (strcmp(de->d_name + namelen - 4, ".idx"))
			continue;

		/* we have .idx.  Is it a file we can map? */
		strcpy(path + len, de->d_name);
		p = add_packed_git(path, len + namelen);
		if (!p)
			continue;
		p->next = packed_git;
		packed_git = p;
	}
	closedir(dir);
}

void prepare_packed_git(void)
{
	static int run_once = 0;
	struct alternate_object_database *alt;

	if (run_once)
		return;
	prepare_packed_git_one(get_object_directory());
	prepare_alt_odb();
	for (alt = alt_odb_list; alt; alt = alt->next) {
		alt->name[0] = 0;
		prepare_packed_git_one(alt->base);
	}
	run_once = 1;
}

int check_sha1_signature(const unsigned char *sha1, void *map, unsigned long size, const char *type)
{
	char header[100];
	unsigned char real_sha1[20];
	SHA_CTX c;

	SHA1_Init(&c);
	SHA1_Update(&c, header, 1+sprintf(header, "%s %lu", type, size));
	SHA1_Update(&c, map, size);
	SHA1_Final(real_sha1, &c);
	return memcmp(sha1, real_sha1, 20) ? -1 : 0;
}

static void *map_sha1_file_internal(const unsigned char *sha1,
				    unsigned long *size)
{
	struct stat st;
	void *map;
	int fd;
	char *filename = find_sha1_file(sha1, &st);

	if (!filename) {
		return NULL;
	}

	fd = open(filename, O_RDONLY | sha1_file_open_flag);
	if (fd < 0) {
		/* See if it works without O_NOATIME */
		switch (sha1_file_open_flag) {
		default:
			fd = open(filename, O_RDONLY);
			if (fd >= 0)
				break;
		/* Fallthrough */
		case 0:
			return NULL;
		}

		/* If it failed once, it will probably fail again.
		 * Stop using O_NOATIME
		 */
		sha1_file_open_flag = 0;
	}
	map = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	if (map == MAP_FAILED)
		return NULL;
	*size = st.st_size;
	return map;
}

int unpack_sha1_header(z_stream *stream, void *map, unsigned long mapsize, void *buffer, unsigned long size)
{
	/* Get the data stream */
	memset(stream, 0, sizeof(*stream));
	stream->next_in = map;
	stream->avail_in = mapsize;
	stream->next_out = buffer;
	stream->avail_out = size;

	inflateInit(stream);
	return inflate(stream, 0);
}

static void *unpack_sha1_rest(z_stream *stream, void *buffer, unsigned long size)
{
	int bytes = strlen(buffer) + 1;
	unsigned char *buf = xmalloc(1+size);

	memcpy(buf, buffer + bytes, stream->total_out - bytes);
	bytes = stream->total_out - bytes;
	if (bytes < size) {
		stream->next_out = buf + bytes;
		stream->avail_out = size - bytes;
		while (inflate(stream, Z_FINISH) == Z_OK)
			/* nothing */;
	}
	buf[size] = 0;
	inflateEnd(stream);
	return buf;
}

/*
 * We used to just use "sscanf()", but that's actually way
 * too permissive for what we want to check. So do an anal
 * object header parse by hand.
 */
int parse_sha1_header(char *hdr, char *type, unsigned long *sizep)
{
	int i;
	unsigned long size;

	/*
	 * The type can be at most ten bytes (including the 
	 * terminating '\0' that we add), and is followed by
	 * a space. 
	 */
	i = 10;
	for (;;) {
		char c = *hdr++;
		if (c == ' ')
			break;
		if (!--i)
			return -1;
		*type++ = c;
	}
	*type = 0;

	/*
	 * The length must follow immediately, and be in canonical
	 * decimal format (ie "010" is not valid).
	 */
	size = *hdr++ - '0';
	if (size > 9)
		return -1;
	if (size) {
		for (;;) {
			unsigned long c = *hdr - '0';
			if (c > 9)
				break;
			hdr++;
			size = size * 10 + c;
		}
	}
	*sizep = size;

	/*
	 * The length must be followed by a zero byte
	 */
	return *hdr ? -1 : 0;
}

void * unpack_sha1_file(void *map, unsigned long mapsize, char *type, unsigned long *size)
{
	int ret;
	z_stream stream;
	char hdr[8192];

	ret = unpack_sha1_header(&stream, map, mapsize, hdr, sizeof(hdr));
	if (ret < Z_OK || parse_sha1_header(hdr, type, size) < 0)
		return NULL;

	return unpack_sha1_rest(&stream, hdr, *size);
}

/* forward declaration for a mutually recursive function */
static int packed_object_info(struct pack_entry *entry,
			      char *type, unsigned long *sizep);

static int packed_delta_info(unsigned char *base_sha1,
			     unsigned long delta_size,
			     unsigned long left,
			     char *type,
			     unsigned long *sizep,
			     struct packed_git *p)
{
	struct pack_entry base_ent;

	if (left < 20)
		die("truncated pack file");

	/* The base entry _must_ be in the same pack */
	if (!find_pack_entry_one(base_sha1, &base_ent, p))
		die("failed to find delta-pack base object %s",
		    sha1_to_hex(base_sha1));

	/* We choose to only get the type of the base object and
	 * ignore potentially corrupt pack file that expects the delta
	 * based on a base with a wrong size.  This saves tons of
	 * inflate() calls.
	 */

	if (packed_object_info(&base_ent, type, NULL))
		die("cannot get info for delta-pack base");

	if (sizep) {
		const unsigned char *data;
		unsigned char delta_head[64];
		unsigned long result_size;
		z_stream stream;
		int st;

		memset(&stream, 0, sizeof(stream));

		data = stream.next_in = base_sha1 + 20;
		stream.avail_in = left - 20;
		stream.next_out = delta_head;
		stream.avail_out = sizeof(delta_head);

		inflateInit(&stream);
		st = inflate(&stream, Z_FINISH);
		inflateEnd(&stream);
		if ((st != Z_STREAM_END) &&
		    stream.total_out != sizeof(delta_head))
			die("delta data unpack-initial failed");

		/* Examine the initial part of the delta to figure out
		 * the result size.
		 */
		data = delta_head;
		get_delta_hdr_size(&data); /* ignore base size */

		/* Read the result size */
		result_size = get_delta_hdr_size(&data);
		*sizep = result_size;
	}
	return 0;
}

static unsigned long unpack_object_header(struct packed_git *p, unsigned long offset,
	enum object_type *type, unsigned long *sizep)
{
	unsigned shift;
	unsigned char *pack, c;
	unsigned long size;

	if (offset >= p->pack_size)
		die("object offset outside of pack file");

	pack =  p->pack_base + offset;
	c = *pack++;
	offset++;
	*type = (c >> 4) & 7;
	size = c & 15;
	shift = 4;
	while (c & 0x80) {
		if (offset >= p->pack_size)
			die("object offset outside of pack file");
		c = *pack++;
		offset++;
		size += (c & 0x7f) << shift;
		shift += 7;
	}
	*sizep = size;
	return offset;
}

void packed_object_info_detail(struct pack_entry *e,
			       char *type,
			       unsigned long *size,
			       unsigned long *store_size,
			       int *delta_chain_length,
			       unsigned char *base_sha1)
{
	struct packed_git *p = e->p;
	unsigned long offset, left;
	unsigned char *pack;
	enum object_type kind;

	offset = unpack_object_header(p, e->offset, &kind, size);
	pack = p->pack_base + offset;
	left = p->pack_size - offset;
	if (kind != OBJ_DELTA)
		*delta_chain_length = 0;
	else {
		int chain_length = 0;
		memcpy(base_sha1, pack, 20);
		do {
			struct pack_entry base_ent;
			unsigned long junk;

			find_pack_entry_one(pack, &base_ent, p);
			offset = unpack_object_header(p, base_ent.offset,
						      &kind, &junk);
			pack = p->pack_base + offset;
			chain_length++;
		} while (kind == OBJ_DELTA);
		*delta_chain_length = chain_length;
	}
	switch (kind) {
	case OBJ_COMMIT:
		strcpy(type, "commit");
		break;
	case OBJ_TREE:
		strcpy(type, "tree");
		break;
	case OBJ_BLOB:
		strcpy(type, "blob");
		break;
	case OBJ_TAG:
		strcpy(type, "tag");
		break;
	default:
		die("corrupted pack file");
	}
	*store_size = 0; /* notyet */
}

static int packed_object_info(struct pack_entry *entry,
			      char *type, unsigned long *sizep)
{
	struct packed_git *p = entry->p;
	unsigned long offset, size, left;
	unsigned char *pack;
	enum object_type kind;
	int retval;

	if (use_packed_git(p))
		die("cannot map packed file");

	offset = unpack_object_header(p, entry->offset, &kind, &size);
	pack = p->pack_base + offset;
	left = p->pack_size - offset;

	switch (kind) {
	case OBJ_DELTA:
		retval = packed_delta_info(pack, size, left, type, sizep, p);
		unuse_packed_git(p);
		return retval;
	case OBJ_COMMIT:
		strcpy(type, "commit");
		break;
	case OBJ_TREE:
		strcpy(type, "tree");
		break;
	case OBJ_BLOB:
		strcpy(type, "blob");
		break;
	case OBJ_TAG:
		strcpy(type, "tag");
		break;
	default:
		die("corrupted pack file");
	}
	if (sizep)
		*sizep = size;
	unuse_packed_git(p);
	return 0;
}

/* forward declaration for a mutually recursive function */
static void *unpack_entry(struct pack_entry *, char *, unsigned long *);

static void *unpack_delta_entry(unsigned char *base_sha1,
				unsigned long delta_size,
				unsigned long left,
				char *type,
				unsigned long *sizep,
				struct packed_git *p)
{
	struct pack_entry base_ent;
	void *data, *delta_data, *result, *base;
	unsigned long data_size, result_size, base_size;
	z_stream stream;
	int st;

	if (left < 20)
		die("truncated pack file");
	data = base_sha1 + 20;
	data_size = left - 20;
	delta_data = xmalloc(delta_size);

	memset(&stream, 0, sizeof(stream));

	stream.next_in = data;
	stream.avail_in = data_size;
	stream.next_out = delta_data;
	stream.avail_out = delta_size;

	inflateInit(&stream);
	st = inflate(&stream, Z_FINISH);
	inflateEnd(&stream);
	if ((st != Z_STREAM_END) || stream.total_out != delta_size)
		die("delta data unpack failed");

	/* The base entry _must_ be in the same pack */
	if (!find_pack_entry_one(base_sha1, &base_ent, p))
		die("failed to find delta-pack base object %s",
		    sha1_to_hex(base_sha1));
	base = unpack_entry_gently(&base_ent, type, &base_size);
	if (!base)
		die("failed to read delta-pack base object %s",
		    sha1_to_hex(base_sha1));
	result = patch_delta(base, base_size,
			     delta_data, delta_size,
			     &result_size);
	if (!result)
		die("failed to apply delta");
	free(delta_data);
	free(base);
	*sizep = result_size;
	return result;
}

static void *unpack_non_delta_entry(unsigned char *data,
				    unsigned long size,
				    unsigned long left)
{
	int st;
	z_stream stream;
	unsigned char *buffer;

	buffer = xmalloc(size + 1);
	buffer[size] = 0;
	memset(&stream, 0, sizeof(stream));
	stream.next_in = data;
	stream.avail_in = left;
	stream.next_out = buffer;
	stream.avail_out = size;

	inflateInit(&stream);
	st = inflate(&stream, Z_FINISH);
	inflateEnd(&stream);
	if ((st != Z_STREAM_END) || stream.total_out != size) {
		free(buffer);
		return NULL;
	}

	return buffer;
}

static void *unpack_entry(struct pack_entry *entry,
			  char *type, unsigned long *sizep)
{
	struct packed_git *p = entry->p;
	void *retval;

	if (use_packed_git(p))
		die("cannot map packed file");
	retval = unpack_entry_gently(entry, type, sizep);
	unuse_packed_git(p);
	if (!retval)
		die("corrupted pack file");
	return retval;
}

/* The caller is responsible for use_packed_git()/unuse_packed_git() pair */
void *unpack_entry_gently(struct pack_entry *entry,
			  char *type, unsigned long *sizep)
{
	struct packed_git *p = entry->p;
	unsigned long offset, size, left;
	unsigned char *pack;
	enum object_type kind;
	void *retval;

	offset = unpack_object_header(p, entry->offset, &kind, &size);
	pack = p->pack_base + offset;
	left = p->pack_size - offset;
	switch (kind) {
	case OBJ_DELTA:
		retval = unpack_delta_entry(pack, size, left, type, sizep, p);
		return retval;
	case OBJ_COMMIT:
		strcpy(type, "commit");
		break;
	case OBJ_TREE:
		strcpy(type, "tree");
		break;
	case OBJ_BLOB:
		strcpy(type, "blob");
		break;
	case OBJ_TAG:
		strcpy(type, "tag");
		break;
	default:
		return NULL;
	}
	*sizep = size;
	retval = unpack_non_delta_entry(pack, size, left);
	return retval;
}

int num_packed_objects(const struct packed_git *p)
{
	/* See check_packed_git_idx() */
	return (p->index_size - 20 - 20 - 4*256) / 24;
}

int nth_packed_object_sha1(const struct packed_git *p, int n,
			   unsigned char* sha1)
{
	void *index = p->index_base + 256;
	if (n < 0 || num_packed_objects(p) <= n)
		return -1;
	memcpy(sha1, (index + 24 * n + 4), 20);
	return 0;
}

int find_pack_entry_one(const unsigned char *sha1,
			struct pack_entry *e, struct packed_git *p)
{
	unsigned int *level1_ofs = p->index_base;
	int hi = ntohl(level1_ofs[*sha1]);
	int lo = ((*sha1 == 0x0) ? 0 : ntohl(level1_ofs[*sha1 - 1]));
	void *index = p->index_base + 256;

	do {
		int mi = (lo + hi) / 2;
		int cmp = memcmp(index + 24 * mi + 4, sha1, 20);
		if (!cmp) {
			e->offset = ntohl(*((int*)(index + 24 * mi)));
			memcpy(e->sha1, sha1, 20);
			e->p = p;
			return 1;
		}
		if (cmp > 0)
			hi = mi;
		else
			lo = mi+1;
	} while (lo < hi);
	return 0;
}

static int find_pack_entry(const unsigned char *sha1, struct pack_entry *e)
{
	struct packed_git *p;
	prepare_packed_git();

	for (p = packed_git; p; p = p->next) {
		if (find_pack_entry_one(sha1, e, p))
			return 1;
	}
	return 0;
}

struct packed_git *find_sha1_pack(const unsigned char *sha1, 
				  struct packed_git *packs)
{
	struct packed_git *p;
	struct pack_entry e;

	for (p = packs; p; p = p->next) {
		if (find_pack_entry_one(sha1, &e, p))
			return p;
	}
	return NULL;
	
}

int sha1_object_info(const unsigned char *sha1, char *type, unsigned long *sizep)
{
	int status;
	unsigned long mapsize, size;
	void *map;
	z_stream stream;
	char hdr[128];

	map = map_sha1_file_internal(sha1, &mapsize);
	if (!map) {
		struct pack_entry e;

		if (!find_pack_entry(sha1, &e))
			return error("unable to find %s", sha1_to_hex(sha1));
		return packed_object_info(&e, type, sizep);
	}
	if (unpack_sha1_header(&stream, map, mapsize, hdr, sizeof(hdr)) < 0)
		status = error("unable to unpack %s header",
			       sha1_to_hex(sha1));
	if (parse_sha1_header(hdr, type, &size) < 0)
		status = error("unable to parse %s header", sha1_to_hex(sha1));
	else {
		status = 0;
		if (sizep)
			*sizep = size;
	}
	inflateEnd(&stream);
	munmap(map, mapsize);
	return status;
}

static void *read_packed_sha1(const unsigned char *sha1, char *type, unsigned long *size)
{
	struct pack_entry e;

	if (!find_pack_entry(sha1, &e)) {
		error("cannot read sha1_file for %s", sha1_to_hex(sha1));
		return NULL;
	}
	return unpack_entry(&e, type, size);
}

void * read_sha1_file(const unsigned char *sha1, char *type, unsigned long *size)
{
	unsigned long mapsize;
	void *map, *buf;
	struct pack_entry e;

	if (find_pack_entry(sha1, &e))
		return read_packed_sha1(sha1, type, size);
	map = map_sha1_file_internal(sha1, &mapsize);
	if (map) {
		buf = unpack_sha1_file(map, mapsize, type, size);
		munmap(map, mapsize);
		return buf;
	}
	return NULL;
}

void *read_object_with_reference(const unsigned char *sha1,
				 const char *required_type,
				 unsigned long *size,
				 unsigned char *actual_sha1_return)
{
	char type[20];
	void *buffer;
	unsigned long isize;
	unsigned char actual_sha1[20];

	memcpy(actual_sha1, sha1, 20);
	while (1) {
		int ref_length = -1;
		const char *ref_type = NULL;

		buffer = read_sha1_file(actual_sha1, type, &isize);
		if (!buffer)
			return NULL;
		if (!strcmp(type, required_type)) {
			*size = isize;
			if (actual_sha1_return)
				memcpy(actual_sha1_return, actual_sha1, 20);
			return buffer;
		}
		/* Handle references */
		else if (!strcmp(type, "commit"))
			ref_type = "tree ";
		else if (!strcmp(type, "tag"))
			ref_type = "object ";
		else {
			free(buffer);
			return NULL;
		}
		ref_length = strlen(ref_type);

		if (memcmp(buffer, ref_type, ref_length) ||
		    get_sha1_hex(buffer + ref_length, actual_sha1)) {
			free(buffer);
			return NULL;
		}
		free(buffer);
		/* Now we have the ID of the referred-to object in
		 * actual_sha1.  Check again. */
	}
}

char *write_sha1_file_prepare(void *buf,
			      unsigned long len,
			      const char *type,
			      unsigned char *sha1,
			      unsigned char *hdr,
			      int *hdrlen)
{
	SHA_CTX c;

	/* Generate the header */
	*hdrlen = sprintf((char *)hdr, "%s %lu", type, len)+1;

	/* Sha1.. */
	SHA1_Init(&c);
	SHA1_Update(&c, hdr, *hdrlen);
	SHA1_Update(&c, buf, len);
	SHA1_Final(sha1, &c);

	return sha1_file_name(sha1);
}

int write_sha1_file(void *buf, unsigned long len, const char *type, unsigned char *returnsha1)
{
	int size;
	unsigned char *compressed;
	z_stream stream;
	unsigned char sha1[20];
	char *filename;
	static char tmpfile[PATH_MAX];
	unsigned char hdr[50];
	int fd, hdrlen, ret;

	/* Normally if we have it in the pack then we do not bother writing
	 * it out into .git/objects/??/?{38} file.
	 */
	filename = write_sha1_file_prepare(buf, len, type, sha1, hdr, &hdrlen);
	if (returnsha1)
		memcpy(returnsha1, sha1, 20);
	if (has_sha1_file(sha1))
		return 0;
	fd = open(filename, O_RDONLY);
	if (fd >= 0) {
		/*
		 * FIXME!!! We might do collision checking here, but we'd
		 * need to uncompress the old file and check it. Later.
		 */
		close(fd);
		return 0;
	}

	if (errno != ENOENT) {
		fprintf(stderr, "sha1 file %s: %s", filename, strerror(errno));
		return -1;
	}

	snprintf(tmpfile, sizeof(tmpfile), "%s/obj_XXXXXX", get_object_directory());

	fd = mkstemp(tmpfile);
	if (fd < 0) {
		fprintf(stderr, "unable to create temporary sha1 filename %s: %s", tmpfile, strerror(errno));
		return -1;
	}

	/* Set it up */
	memset(&stream, 0, sizeof(stream));
	deflateInit(&stream, Z_BEST_COMPRESSION);
	size = deflateBound(&stream, len+hdrlen);
	compressed = xmalloc(size);

	/* Compress it */
	stream.next_out = compressed;
	stream.avail_out = size;

	/* First header.. */
	stream.next_in = hdr;
	stream.avail_in = hdrlen;
	while (deflate(&stream, 0) == Z_OK)
		/* nothing */;

	/* Then the data itself.. */
	stream.next_in = buf;
	stream.avail_in = len;
	while (deflate(&stream, Z_FINISH) == Z_OK)
		/* nothing */;
	deflateEnd(&stream);
	size = stream.total_out;

	if (write(fd, compressed, size) != size)
		die("unable to write file");
	fchmod(fd, 0444);
	close(fd);
	free(compressed);

	ret = link(tmpfile, filename);
	if (ret < 0) {
		ret = errno;

		/*
		 * Coda hack - coda doesn't like cross-directory links,
		 * so we fall back to a rename, which will mean that it
		 * won't be able to check collisions, but that's not a
		 * big deal.
		 *
		 * When this succeeds, we just return 0. We have nothing
		 * left to unlink.
		 */
		if (ret == EXDEV && !rename(tmpfile, filename))
			return 0;
	}
	unlink(tmpfile);
	if (ret) {
		if (ret != EEXIST) {
			fprintf(stderr, "unable to write sha1 filename %s: %s", filename, strerror(ret));
			return -1;
		}
		/* FIXME!!! Collision check here ? */
	}

	return 0;
}

int write_sha1_to_fd(int fd, const unsigned char *sha1)
{
	ssize_t size;
	unsigned long objsize;
	int posn = 0;
	void *map = map_sha1_file_internal(sha1, &objsize);
	void *buf = map;
	void *temp_obj = NULL;
	z_stream stream;

	if (!buf) {
		unsigned char *unpacked;
		unsigned long len;
		char type[20];
		char hdr[50];
		int hdrlen;
		// need to unpack and recompress it by itself
		unpacked = read_packed_sha1(sha1, type, &len);

		hdrlen = sprintf(hdr, "%s %lu", type, len) + 1;

		/* Set it up */
		memset(&stream, 0, sizeof(stream));
		deflateInit(&stream, Z_BEST_COMPRESSION);
		size = deflateBound(&stream, len + hdrlen);
		temp_obj = buf = xmalloc(size);

		/* Compress it */
		stream.next_out = buf;
		stream.avail_out = size;
		
		/* First header.. */
		stream.next_in = (void *)hdr;
		stream.avail_in = hdrlen;
		while (deflate(&stream, 0) == Z_OK)
			/* nothing */;

		/* Then the data itself.. */
		stream.next_in = unpacked;
		stream.avail_in = len;
		while (deflate(&stream, Z_FINISH) == Z_OK)
			/* nothing */;
		deflateEnd(&stream);
		free(unpacked);
		
		objsize = stream.total_out;
	}

	do {
		size = write(fd, buf + posn, objsize - posn);
		if (size <= 0) {
			if (!size) {
				fprintf(stderr, "write closed");
			} else {
				perror("write ");
			}
			return -1;
		}
		posn += size;
	} while (posn < objsize);

	if (map)
		munmap(map, objsize);
	if (temp_obj)
		free(temp_obj);

	return 0;
}

int write_sha1_from_fd(const unsigned char *sha1, int fd, char *buffer,
		       size_t bufsize, size_t *bufposn)
{
	char *filename = sha1_file_name(sha1);

	int local;
	z_stream stream;
	unsigned char real_sha1[20];
	unsigned char discard[4096];
	int ret;
	SHA_CTX c;

	local = open(filename, O_WRONLY | O_CREAT | O_EXCL, 0666);

	if (local < 0)
		return error("Couldn't open %s\n", filename);

	memset(&stream, 0, sizeof(stream));

	inflateInit(&stream);

	SHA1_Init(&c);

	do {
		ssize_t size;
		if (*bufposn) {
			stream.avail_in = *bufposn;
			stream.next_in = (unsigned char *) buffer;
			do {
				stream.next_out = discard;
				stream.avail_out = sizeof(discard);
				ret = inflate(&stream, Z_SYNC_FLUSH);
				SHA1_Update(&c, discard, sizeof(discard) -
					    stream.avail_out);
			} while (stream.avail_in && ret == Z_OK);
			write(local, buffer, *bufposn - stream.avail_in);
			memmove(buffer, buffer + *bufposn - stream.avail_in,
				stream.avail_in);
			*bufposn = stream.avail_in;
			if (ret != Z_OK)
				break;
		}
		size = read(fd, buffer + *bufposn, bufsize - *bufposn);
		if (size <= 0) {
			close(local);
			unlink(filename);
			if (!size)
				return error("Connection closed?");
			perror("Reading from connection");
			return -1;
		}
		*bufposn += size;
	} while (1);
	inflateEnd(&stream);

	close(local);
	SHA1_Final(real_sha1, &c);
	if (ret != Z_STREAM_END) {
		unlink(filename);
		return error("File %s corrupted", sha1_to_hex(sha1));
	}
	if (memcmp(sha1, real_sha1, 20)) {
		unlink(filename);
		return error("File %s has bad hash\n", sha1_to_hex(sha1));
	}
	
	return 0;
}

int has_pack_index(const unsigned char *sha1)
{
	struct stat st;
	if (stat(sha1_pack_index_name(sha1), &st))
		return 0;
	return 1;
}

int has_pack_file(const unsigned char *sha1)
{
	struct stat st;
	if (stat(sha1_pack_name(sha1), &st))
		return 0;
	return 1;
}

int has_sha1_pack(const unsigned char *sha1)
{
	struct pack_entry e;
	return find_pack_entry(sha1, &e);
}

int has_sha1_file(const unsigned char *sha1)
{
	struct stat st;
	struct pack_entry e;

	if (find_pack_entry(sha1, &e))
		return 1;
	return find_sha1_file(sha1, &st) ? 1 : 0;
}

int index_fd(unsigned char *sha1, int fd, struct stat *st, int write_object, const char *type)
{
	unsigned long size = st->st_size;
	void *buf;
	int ret;
	unsigned char hdr[50];
	int hdrlen;

	buf = "";
	if (size)
		buf = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	if (buf == MAP_FAILED)
		return -1;

	if (!type)
		type = "blob";
	if (write_object)
		ret = write_sha1_file(buf, size, type, sha1);
	else {
		write_sha1_file_prepare(buf, size, type, sha1, hdr, &hdrlen);
		ret = 0;
	}
	if (size)
		munmap(buf, size);
	return ret;
}
