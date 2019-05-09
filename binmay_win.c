/* {{{ license

Binmay - command line binary search
Copyright (C) 2004 - 2011 Sean Loaring
Copyright (C) 2019 Slore

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

Email: info@filewut.com

Web Site: http://www.filewut.com/spages/page.php/software/binmay

Changes:


050111: added patch from Bill Poser that allows input to be binary rather than hex

}}} */
// {{{ include
#include <stdio.h>
#include <sys/unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
// }}}
// {{{ defines
//#define DEBUG(x) { x; };
#define DEBUG(x) ;

#define BUF_LEN 1024
#define BLOCK_LEN 1024
// }}}

int verbose = 0;
int UseBinaryP = 0;
char pukebuf[BUF_LEN];
int plen = -1;
int matches = 0;

// {{{ structs
struct masked_string { // {{{
    char string[BUF_LEN];
    int length;
    char mask[BUF_LEN];
    int masklength;
}; // }}}
struct file_handle { // {{{
    char filename[BUF_LEN];
    FILE *file;
}; // }}}
struct buffer { // {{{
    char buf[BLOCK_LEN * 2];
    int length;
    int size;
    int coffset; //number of bytes that have gone through buffer already
}; // }}}
struct buffered_handle { // {{{
    struct buffer *buf;
    struct file_handle *fh;
}; // }}}
   // }}}
   // {{{ protos
void use();

void open_outfile(char *filename);
void open_infile(char *filename);

void flush_outbuf();
void flush_outbuf_block();

int b_getchar();
void do_search_replace();
void fill_ibuf();
void b_fwrite(unsigned char *buf, int len);
size_t process_string(char *dst, size_t dstlen, char *src, size_t srclen);
void buffered_skip(struct buffered_handle *h, size_t skip);
void buffered_setfile(struct buffered_handle *bh, char *filename, FILE *def, const char *mode);

void file_handle_setfile(struct file_handle *fh, char *filename, FILE *def, const char *mode);

struct masked_string * masked_string_new();
void masked_string_setstr(struct masked_string *masked, char *str);
void masked_string_setmask(struct masked_string *masked, char *mask);
void masked_string_ensuremask(struct masked_string *masked);
void masks_check(struct masked_string *search, struct masked_string *replace);
int masked_string_isset(struct masked_string *masked);

struct buffered_handle *buffered_handle_new(char *filename, FILE *def, const char *mode);
struct file_handle *file_handle_new(char *filename, FILE *def, const char *mode);
// }}}

/* getopt function() from LCC for Windows */
/******************************************/
#include  <stdio.h>
#include  <string.h>

#define ERR(str, chr)       if(opterr){fprintf(stderr, "%s%c\n", str, chr);}

int     opterr = 1;
int     optind = 1;
int optopt;
char *optarg;

int
getopt(int argc, char *const argv[], const char *opts)
{
    static int sp = 1;
    int c;
    const char *cp;

    if (sp == 1) {
        if (optind >= argc ||
            argv[optind][0] != '-' || argv[optind][1] == '\0')
            return -1;
        else if (strcmp(argv[optind], "--") == 0) {
            optind++;
            return -1;
        }
    }
    optopt = c = argv[optind][sp];
    if (c == ':' || (cp = strchr(opts, c)) == 0) {
        ERR(": illegal option -- ", c);
        if (argv[optind][++sp] == '\0') {
            optind++;
            sp = 1;
        }
        return c; //'?';
    }
    if (*++cp == ':') {
        if (argv[optind][sp + 1] != '\0')
            optarg = &argv[optind++][sp + 1];
        else if (++optind >= argc) {
            ERR(": option requires an argument -- ", c);
            sp = 1;
            return c; //'?';
        } else
            optarg = argv[optind++];
        sp = 1;
    } else {
        if (argv[optind][++sp] == '\0') {
            sp = 1;
            optind++;
        }
        optarg = 0;
    }
    return c;
}
/******************************************/

void set_search_str(struct masked_string *search, char *str)
{
    int i = 0;
    char chr = '\0';
    char buff[BUF_LEN] = {0};
    masked_string_setstr(search, str);
    /* case insensitive search */
    if (strlen(str) >= 2 && ':' == str[1] && 'T' == str[0]) {
        for (i = 0;i < search->length; i++) {
            chr = search->string[i];
            if ((chr >= 'a' && chr <= 'z') || (chr >= 'A' && chr <= 'Z')) {
                search->mask[i] = 0xdf;
            } else {
                search->mask[i] = 0xff;
            }
        }
        search->masklength = search->length;
    }
}

int main(int argc, char **argv) // {{{
{
    char c;

    struct buffered_handle *in = buffered_handle_new("-", stdin, "rb");
    struct file_handle *out = file_handle_new("-", stdout, "wb");

    struct masked_string *search = masked_string_new();
    struct masked_string *replace = masked_string_new();

    //	infile=stdin;
    //	outfile=stdout;

    char options[] = "bvp:S:R:i:o:s:r:";
    if (1 == argc) {
        use();
        return 0;
    }

    while ((c = getopt(argc, argv, options)) > 0) {
        switch (c) {
        case 'b':
            UseBinaryP = 1;
            break;
        case 'i':
            buffered_setfile(in, optarg, stdin, "rb");
            break;
        case 'o':
            file_handle_setfile(out, optarg, stdout, "wb");
            break;
        case 'r':
            masked_string_setstr(replace, optarg);
            break;
        case 's':
            set_search_str(search, optarg);
            break;
        case 'v':
            verbose++;
            break;

        case 'S':
            masked_string_setmask(search, optarg);
            break;

        case 'R':
            masked_string_setmask(replace, optarg);
            break;

        case 'p':
            if (strlen(optarg) > BUF_LEN) {
                fprintf(stderr, "The replace mask is longer than the buffer :(\n");
                fprintf(stderr, "You might have to modify BUF_LEN and recompile...sorry!\n");
                exit(1);
            }
            plen = process_string(pukebuf, BUF_LEN, optarg, strlen(optarg));
            /*
            strncpy( pukebuf, optarg, BUF_LEN );
            process_string( pukebuf, &plen );
            */
            break;

        default:
        {
            fprintf(stderr, "Command line parser: I don't understand option '-%c'.\n",
                (unsigned int)c);
            fprintf(stderr, "Run with no options for help\n");
            exit(1);
        }
        }
    }


    masked_string_ensuremask(search);
    masks_check(search, replace);

    if (masked_string_isset(search) && -1 != plen) {
        perror("I don't know how to puke and search at the same time.");
    }

    if (masked_string_isset(search)) {
        do_search_replace(in, out, search, replace);

        if (!masked_string_isset(replace)) {
            printf("Matches: %d\n", matches);
        }

        if (verbose) fprintf(stderr, " Done: %d matches.\n", matches);
    }

    if (-1 != plen) {
        if (verbose)fprintf(stderr, "Puking binary to output\n");
        fflush(stderr);
        if (1 != fwrite(pukebuf, plen, 1, out->file)) {
            fprintf(stderr, "Write error: \"%s\" :(\n", strerror(errno));
            exit(1);
        }
    }

    return 0;
} // }}}

void *f_malloc(size_t size) // {{{ returns malloc()d buffer or aborts
{
    void *r;
    if (NULL == (r = (struct input*)malloc(size))) {
        fprintf(stderr, "Error allocating %d bytes.\n", size);
        abort();
    }

    return r;
} // }}}

struct buffer *buffer_new() // {{{
{
    struct buffer *r;

    r = (struct buffer*)f_malloc(sizeof(struct buffer));

    r->length = 0;
    r->size = BLOCK_LEN * 2;
    r->coffset = 0;

    return r;
} // }}}

void file_handle_setfile(struct file_handle *fh, char *filename, FILE *def, const char *mode) // {{{
{
    if (strlen(filename) > BUF_LEN) {
        fprintf(stderr, "Input file name too large for buffer.\n");
        abort();
    }

    if (0 == strcmp(filename, "-")) {
        fh->file = def;
    } else {
        if (verbose) fprintf(stderr, "Writing to %s...\n", filename);
        if (NULL == (fh->file = fopen(filename, mode))) {
            fprintf(stderr, "Error opening file %s\n", filename);
            exit(1);
        }
    }
} // }}}
struct file_handle *file_handle_new(char *filename, FILE *def, const char *mode) // {{{
{
    struct file_handle *r;

    r = (struct file_handle*)f_malloc(sizeof(struct file_handle));
    file_handle_setfile(r, filename, def, mode);
    return r;
} // }}}

void buffered_setfile(struct buffered_handle *bh, char *filename, FILE *def, const char *mode) // {{{
{
    file_handle_setfile(bh->fh, filename, def, mode);
} // }}}
struct buffered_handle *buffered_handle_new(char *filename, FILE *def, const char *mode) // {{{
{
    struct buffered_handle *r;

    r = (struct buffered_handle*)f_malloc(sizeof(struct buffered_handle));

    r->fh = file_handle_new(filename, def, mode);
    r->buf = buffer_new();

    return r;

} // }}}

struct masked_string * masked_string_new() // {{{
{
    struct masked_string *r = f_malloc(sizeof(struct masked_string));

    r->length = -1;
    r->masklength = -1;

    return r;
} // }}}
void masked_string_setstr(struct masked_string *masked, char *str) // {{{
{
    if (strlen(str) > BUF_LEN) {
        fprintf(stderr, "Mask string is longer than buffer.\n");
        exit(1);
    }

    masked->length = process_string(masked->string, BUF_LEN, str, strlen(str));

    DEBUG(fprintf(stderr, "Setting masked string >%s< -> >%s<, %d\n", str, masked->string, masked->length));

    /*
    strncpy( masked->string, str, BUF_LEN );
    process_string( masked->string, &(masked->length ) );
    */
} // }}}
void masked_string_setmask(struct masked_string *masked, char *mask) // {{{
{
    if (strlen(mask) > BUF_LEN) {
        fprintf(stderr, "Mask string is longer than buffer.\n");
        exit(1);
    }

    masked->masklength = process_string(masked->mask, BUF_LEN, mask, strlen(mask));

    /*
    strncpy( masked->mask, mask, BUF_LEN );
    process_string( masked->mask, &(masked->masklength ) );
    */
} // }}}
void masked_string_ensuremask(struct masked_string *masked) // {{{ If a mask hasn't been set then set it to 0xff * length
{
    if (masked->length > 0 && -1 == masked->masklength) {
        masked->masklength = masked->length;
        memset(masked->mask, 0xff, masked->length);
    }

    if (masked->length != masked->masklength) {
        fprintf(stderr, "Search mask length (%d) != search string length (%d).\n",
                masked->length, masked->masklength);
        exit(1);
    }
} // }}}
int masked_string_isset(struct masked_string *masked) // {{{
{
    if (-1 == masked->length)
        return 0;
    else
        return 1;
} // }}}
int masked_string_match(struct masked_string *masked, size_t offset, char *buf) // {{{
{
    int i;
    for (i = 0; i < masked->length; i++) {
        char chr = buf[i];
        if ((chr & masked->mask[i]) != (masked->string[i] & masked->mask[i])) {
            return 0;
        }
    }

    matches++;

    return 1;

} // }}}
int masked_string_seek(struct masked_string *masked, struct buffer *buf) // {{{ search for buf in masked; out: offset or -1
{
    int i;
    for (i = 0; buf->length - i >= masked->length; i++) {
        if (masked_string_match(masked, i, buf->buf + i)) {
            return i;
        }
    }

    return -1;
} // }}}
void masks_check(struct masked_string *search, struct masked_string *replace) // {{{
{
    if (-1 != search->length && -1 != replace->length &&
        (replace->masklength > replace->length || replace->masklength > search->length)) {
        fprintf(stderr,
                "Replace mask is longer than either either the search string or replacement string,\n"
                "ignoring extra mask.\n"
        );
        replace->masklength = (replace->length > search->length) ? search->length : replace->length;
    }
} // }}}

void hexdump(FILE *out, char *buf, int len) // {{{
{
    int i = 0;

    for (i = 0; i < len; i++) {
        fprintf(out, "%02x ", (unsigned char)buf[i]);
    }

    fprintf(out, "\n");
} // }}}

void hexdumpline(FILE *out, char *buf, int len) // {{{
{
    int i = 0;

    for (i = 0; i < len; i++) {
        fprintf(out, "%02x ", (unsigned char)buf[i]);
    }
} // }}}

void buffer_flush(struct buffered_handle *in, struct file_handle *out, size_t length) // {{{ flush length bytes from in to out
{

    if (0 == length) return;
    if (in->buf->length < length) {
        fprintf(stderr, "Attempt to overrun buffer.  Please report this bug.\n");
        abort();
    }

    if (1 != fwrite(in->buf->buf, length, 1, out->file)) {
        fprintf(stderr, "Error writing %d bytes: %s\n", length, strerror(errno));
        abort();
    }


    buffered_skip(in, length);

} // }}}
void masked_replace(struct buffered_handle *in, struct file_handle *out, struct masked_string *replace) // {{{
{
    char replace_buf[BUF_LEN * 2];
    if (-1 == replace->masklength) {
        if (replace->length > 0) {
            if (1 != fwrite(replace->string, replace->length, 1, out->file)) {
                fprintf(stderr, "Error writing %d bytes: %s\n", replace->length, strerror(errno));
                abort();
            }
        }
    } else {
        if (in->buf->length < replace->masklength) {
            fprintf(stderr, "Input buffer (%d) is not long enough for replacement (%d).\n",
                    in->buf->length, replace->masklength);
            exit(1);
        }

        int i;

        for (i = 0; i < replace->length; i++) {
            if (i < replace->masklength) {
                replace_buf[i] = ((replace->string[i] & replace->mask[i]) | (in->buf->buf[i] & (~replace->mask[i])));
            } else {
                replace_buf[i] = replace->string[i];
            }
        }

        if (1 != fwrite(replace_buf, replace->length, 1, out->file)) {
            fprintf(stderr, "Error writing %d bytes: %s\n", replace->length, strerror(errno));
            abort();
        }
    }

} // }}}
void buffered_skip(struct buffered_handle *h, size_t skip) // {{{
{
    if (skip < 0 || skip > h->buf->length) {
        fprintf(stderr, "Tried to advance buffer out of range %d by %d bytes.\n",
                h->buf->length, skip);
        abort();
    }



    memmove(h->buf->buf, h->buf->buf + skip, h->buf->length - skip);

    h->buf->length -= skip;
    h->buf->coffset += skip;
} // }}}
int buffered_read(struct buffered_handle *h, int length) // {{{
{
    if (length > h->buf->size - h->buf->length) {
        fprintf(stderr, "Attempt to read (%d) more than there was room for (%d - %d = %d).\n",
                length, h->buf->size, h->buf->length, h->buf->size - h->buf->length);
        abort();
    }

    int i = fread(h->buf->buf + h->buf->length, 1, length, h->fh->file);

    if (i < 0) {
        fprintf(stderr, "Read error: %s\n", strerror(errno));
        exit(1);
    }

    h->buf->length += i;

    return i;

} // }}}
void do_search_replace(struct buffered_handle *in, struct file_handle *out, struct masked_string *search, struct masked_string *replace) // {{{
{

    while (buffered_read(in, BUF_LEN)) {
        while (in->buf->length >= search->length) {
            int found;
            if (-1 != (found = masked_string_seek(search, in->buf))) {
                if (masked_string_isset(replace)) {
                    buffer_flush(in, out, found);
                    masked_replace(in, out, replace);
                } else {
                    buffered_skip(in, found);
                    printf("0x%x:", in->buf->coffset);
                    hexdumpline(stdout, in->buf->buf, search->length);
                    printf("\n");
                }
                buffered_skip(in, search->length);
            } else { // couldn't find the search string in the buffer, skip ahead
                if (masked_string_isset(replace)) {
                    buffer_flush(in, out, in->buf->length - search->length + 1);
                } else {
                    buffered_skip(in, in->buf->length - search->length + 1);
                }
            }
        }
    }


    if (masked_string_isset(replace)) {
        buffer_flush(in, out, in->buf->length);
    } else {
        buffered_skip(in, in->buf->length);
    }


} // }}}

int hval(char c) // {{{ in: char '0', '1', ... , 'f'; out: int 0, 1, ... , 15
{
    switch (c) {
    case '0': return 0;
    case '1': return 1;
    case '2':return 2;
    case '3':return 3;
    case '4':return 4;
    case '5':return 5;
    case '6':return 6;
    case '7':return 7;
    case '8':return 8;
    case '9':return 9;
    case 'A':return 10;
    case 'B':return 11;
    case 'C':return 12;
    case 'D':return 13;
    case 'E':return 14;
    case 'F':return 15;
    default:
        fprintf(stderr, "INTERNAL ERROR: hval() got something weird: \"%c\" == %d \n",
            (unsigned int)c, (unsigned int)c);
        exit(1);

    }
} // }}}
int isbdigit(char x) // {{{
{
    switch (x) {
    case '0':
    case '1':
        return (1);
    default:
        return(0);
    }
} // }}}
size_t process_bin_string(char *dst, size_t dstlen, char *src, size_t srclen) // {{{ convert ascii binary to bytes
{
    int off;
    int noff;

    char cleanbuf[BUF_LEN];
    size_t nclean = 0;
    int r = 0;

    for (off = 0; off < srclen; off++) {
        //clean out crap
        if (isbdigit(src[off])) {
            if (nclean >= sizeof(cleanbuf) - 1) {
                fprintf(stderr, "Clean binary string is larger than buffer.\n");
                exit(1);
            }
            cleanbuf[nclean] = src[off];
            nclean++;
        }
        /*
        while( src[off] != 0 && ! isbdigit(str[off]) ) {
        for ( noff=off;src[noff] !=0; noff++ ){
        dst[noff] = src[noff+1];
        }
        }
        */
    }
    cleanbuf[nclean] = '\0';
    DEBUG(fprintf(stderr, "Cleanstr: %s\n", cleanbuf); )

        //*len = strlen(str); 
        if (nclean % 8 != 0) {
            fprintf(stderr, "Error: invalid binary string: %s, length must be a multiple of 8.\n", src);
            exit(1);
        }

    r = nclean / 8;
    //*len = *len/8;
    DEBUG(fprintf(stderr, "result len: %d\n", r));

    if (r > dstlen) {
        fprintf(stderr, "Error: destination buffer is too small.\n");
        exit(1);
    }

    for (noff = 0; noff * 8 < nclean; noff++) {
        int start = noff * 8;
        dst[noff] =
            ((cleanbuf[start] - 0x30) << 7) +
            ((cleanbuf[start + 1] - 0x30) << 6) +
            ((cleanbuf[start + 2] - 0x30) << 5) +
            ((cleanbuf[start + 3] - 0x30) << 4) +
            ((cleanbuf[start + 4] - 0x30) << 3) +
            ((cleanbuf[start + 5] - 0x30) << 2) +
            ((cleanbuf[start + 6] - 0x30) << 1) +
            cleanbuf[start + 7] - 0x30;

        /*
        DEBUG(fprintf( stderr, "noff: %d\n", noff );)
        DEBUG(fprintf(stderr, "A val: %x\n", (unsigned int)dst[noff]);)
        */


        DEBUG(
            fprintf(stderr, "%d %c%c%c%c%c%c%c%c -> ",
                    noff,
                    cleanbuf[start],
                    cleanbuf[start + 1],
                    cleanbuf[start + 2],
                    cleanbuf[start + 3],
                    cleanbuf[start + 4],
                    cleanbuf[start + 5],
                    cleanbuf[start + 6],
                    cleanbuf[start + 7]);
        )


            DEBUG(fprintf(stderr, "A val: %x\n", (unsigned int)(unsigned char)dst[noff]);)
    }

    DEBUG(
        fprintf(stderr, "process_bin_string() >%s< ->", src);
    hexdumpline(stderr, dst, r);
    fprintf(stderr, "\n");
    );

    return r;
} // }}}
size_t process_hex_string(char *dst, size_t dstlen, char *src, size_t srclen) // {{{
{
    int off;
    int noff;
    int r;

    char cleanbuf[BUF_LEN];
    size_t nclean = 0;


    for (off = 0; off < srclen; off++) {
        //clean out crap
        if (isxdigit(src[off])) {
            if (nclean >= sizeof(cleanbuf) - 1) {
                fprintf(stderr, "Clean binary string is larger than buffer.\n");
                exit(1);
            }
            cleanbuf[nclean] = toupper(src[off]);
            nclean++;
        }
    }

    /*
    for( off=0; off<srclen; off++ ) {
    //clean out crap
    if( isxdigit( src[off] ) ) {

    }
    while( str[off] != 0 && ! isxdigit( str[off] ) ) {
    for ( noff=off;str[noff] !=0; noff++ ) {
    str[noff] = str[noff+1];
    }
    }
    //ucase the string
    str[off] = toupper(str[off]);
    }
    */

    //*len = strlen( str );

    cleanbuf[nclean] = '\0';
    DEBUG(fprintf(stderr, "Cleanstr(%d): >%s< -> >%s<\n", nclean, src, cleanbuf); )

        //convert to values
        if (nclean % 2 != 0) {
            fprintf(stderr, "Error: invalid hex string: %s, length must be a multiple of two.\n", src);
            exit(1);
        }

    //*len = *len/2;
    r = nclean / 2;

    if (r > dstlen) {
        fprintf(stderr, "Error: decoded hex would overrun buffer.\n");
        exit(1);
    }

    DEBUG(fprintf(stderr, "len: %d\n", r);)

        for (noff = 0; noff < r; noff++) {
            dst[noff] = (hval(cleanbuf[noff * 2]) << 4) + hval(cleanbuf[noff * 2 + 1]);

            DEBUG(fprintf(stderr, "noff: %d\n", noff);)
                DEBUG(fprintf(stderr, "A val: %x\n", (unsigned int)cleanbuf[noff]);)
        }

    DEBUG(fprintf(stderr, "process_string() Done w/%d\n", r); );

    return r;
} // }}}
size_t load_file(char *path, char *dst, size_t dstlen) // {{{
{
    FILE *fp;
    if (0 == strcmp(path, "-")) {
        fp = stdin;
    } else if (NULL == (fp = fopen(path, "rb"))) {
        fprintf(stderr, "Error reading from file \"%s\": %s\n", path, strerror(errno));
        exit(1);
    }

    int r = fread(dst, 1, dstlen, fp);

    if (r == dstlen && !feof(fp)) {
        fprintf(stderr, "Input file %s is larger than buffer length %d\n", path, dstlen);
        fclose(fp);
        exit(1);
    }

    fclose(fp);

    return r;
} // }}}
size_t process_string(char *dst, size_t dstlen, char *src, size_t srclen) // {{{ processes src, stores result in dst, returns length
{
    if (strlen(src) >= 2 && ':' == src[1]) {
        switch (src[0]) {
        case 'f':
            return load_file(src + 2, dst, dstlen);
            break;
        case 't':
        case 'T':
            if (dstlen < srclen - 2) {
                fprintf(stderr, "Can't use >%s<, buffer overrun.\n", src);
                exit(1);
            }
            memcpy(dst, src + 2, srclen - 2);
            return srclen - 2;
            break;
        case 'h':
            return process_hex_string(dst, dstlen, src + 2, srclen - 2);

        case 'b':
            return process_bin_string(dst, dstlen, src + 2, srclen - 2);
        default:
            fprintf(stderr, "Unknown format prefix: %c\n", src[0]);
            exit(1);
        }
    } else {
        if (UseBinaryP) {
            return process_bin_string(dst, dstlen, src, srclen);
        } else {
            return process_hex_string(dst, dstlen, src, srclen);
        }
    }
} // }}}
void use() // {{{
{
    fprintf(stderr,
            "use: binmay [options] [-i infile] [-o outfile] [-s search] [-r replacement]\n"
            "      search:        the string to search for\n"
            "      replacement:   the string to replace \"search\" with \n"
            "  options:\n"
            "      -v             verbose\n"
            "      -b             use binary rather than hex (obsolete)\n"
            "      -i [infile]    specify input file (default: stdin)\n"
            "      -p [string]    puke raw binary\n"
            "      -o [outfile]   specify output file (default: stdout)\n"
            "      -s [string]    string to search for (in hex, see below)\n"
            "      -r [string]    replacement string (in hex, see below)\n"
            "      -S [string]    search mask (see readme)\n"
            "      -R [string]    replace mask (see readme)\n"
            "\n"
            "  string format:\n"
            "      By default search/replace/mask strings are treated as hex.\n"
            "      Non-hex characters are ignored.  You can have strings\n"
            "      treated as binary by using the -b switch.  You can also\n"
            "      specify different formats as follows:\n"
            "\n"
            "      b:binary\n"
            "      t:text\n"
            "      T:text (auto set mask for case insensitive searching)\n"
            "      h:hex\n"
            "      f:file_input\n"
            "\n"
            "  Examples:\n"
            "    Replace all instances of \"cows\" in the file \"cowfile\"\n"
            "    with \"x\" and output to \"bfile\"\n"
            "      binmay -s t:cows -r b:01111000 -i cowfile -o bfile\n"
            "\n"
            "    Replace all instances of 0xff00 with 0x1212:\n"
            "      binmay -s ff00 -r 1212\n"
            "\n"
            "    Use masking to replace all instances of 0xffXX with 0x12XX,\n"
            "    where XX is any value:\n"
            "      binmay -s ff00 -S 00ff -r 1212\n"
            "\n"
            "  See README file for more information and examples\n"

    );
} // }}}
