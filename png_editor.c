#include "png_editor.h"

#define FILENAME "image.png"

extern unsigned long crc(unsigned char *buf, int len);

/* counting chunks in png image data
 *  \param[in]  buf image data.
 *  \param[in]  size of buf in byte.
 *  \return how many chunks includes.
 */
int counting_chunks( unsigned char *buf, int len ) {
    unsigned char *p;
    int count, temp;

    count = 0;
    p = &buf[8];
    do {
        count++;

        temp = p[0];
        temp <<= 8;
        temp += p[1];
        temp <<= 8;
        temp += p[2];
        temp <<= 8;
        temp += p[3];

        p += 12 + temp;
    } while ( (p - buf) < len );
    return count;
}

/*  split image data into Chunk array
 *  \param[in]  buf image data.
 *  \param[in]  len size of buf in byte.
 *  \return how many chunks got.
 */
int chunk_split( unsigned char *buf, int len, Chunk **chunks ) {
    unsigned char *p;
    int ret;

    p = &buf[8];

    if ( strncmp( (char*)&p[4], "IHDR", 4 ) )
        return -1;

    *chunks = (Chunk*)calloc( counting_chunks(buf, len), sizeof(Chunk) );

    (*chunks)[0].length = p[0];
    (*chunks)[0].length <<= 8;
    (*chunks)[0].length += p[1];
    (*chunks)[0].length <<= 8;
    (*chunks)[0].length += p[2];
    (*chunks)[0].length <<= 8;
    (*chunks)[0].length += p[3];

    (*chunks)[0].type[0] = (char)p[4];
    (*chunks)[0].type[1] = (char)p[5];
    (*chunks)[0].type[2] = (char)p[6];
    (*chunks)[0].type[3] = (char)p[7];
    (*chunks)[0].type[4] = 0;

    (*chunks)[0].data = &p[8];

    (*chunks)[0].crc = &((*chunks)[0]).data[(*chunks)[0].length];

    ret = 1;
    do {
        p += ( 12 + (*chunks)[ret-1].length );

        (*chunks)[ret].length = p[0];
        (*chunks)[ret].length <<= 8;
        (*chunks)[ret].length += p[1];
        (*chunks)[ret].length <<= 8;
        (*chunks)[ret].length += p[2];
        (*chunks)[ret].length <<= 8;
        (*chunks)[ret].length += p[3];

        (*chunks)[ret].type[0] = (char)p[4];
        (*chunks)[ret].type[1] = (char)p[5];
        (*chunks)[ret].type[2] = (char)p[6];
        (*chunks)[ret].type[3] = (char)p[7];
        (*chunks)[ret].type[4] = 0;

        (*chunks)[ret].data = &p[8];

        (*chunks)[ret].crc = &((*chunks)[ret]).data[(*chunks)[ret].length];

        ret++;
    } while ( (strncmp( (char*)&p[4], "IEND", 4 )) && (p < &buf[len]) );

    return ret;
}

int check_crc( Chunk* data ) {
    int s;
    s = data->crc[0];
    s <<= 8;
    s += data->crc[1];
    s <<= 8;
    s += data->crc[2];
    s <<= 8;
    s += data->crc[3];

    if ( s == crc( data->data-4, data->length+4 ) )
        return 1;
    else
        return 0;
}

void main_pen_editor( int argc, char **argv ) {
    int fd, ret, temp;
    unsigned char *buf, *p;
    unsigned char png_signature[8] = { 0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a };
    struct stat st;
	Chunk *chunks;

    fd = open( FILENAME, O_RDONLY );
    if ( fd == -1 ) {
        printf( "failed to open file: %s : %s\n", FILENAME, strerror(errno) );
        return;
    }

    ret = fstat( fd, &st );
    if ( ret ) {
        printf( "failed to get file(%s) stat : %s\n", FILENAME, strerror(errno) );
    }

    buf = (unsigned char*)calloc( st.st_size, sizeof(unsigned char) );
    if ( !buf )
        exit(1);

    p = buf;
    ret = 0;
    while ( ret < st.st_size ) {
        ret += read( fd, p, st.st_size-ret );
        p += ret;
    }
    close(fd);

    printf( "read: %d bytes.\n", ret );

    if ( strncmp( (char*)buf, (char*)png_signature, 8 ) ) {
        printf( "%s: not a valid png image file.\n", FILENAME );
        exit(1);
    }

    ret = chunk_split( buf, ret, &chunks );
    if ( ret == -1 )
        exit(1);

    for ( temp=0; temp<ret; temp++ ) {
        printf( "-----------------------\nchunk length: %.8x\nchunk type: %s\nchunk crc: %.2x %.2x %.2x %.2x\n", 
                chunks[temp].length, chunks[temp].type, 
                chunks[temp].crc[0], chunks[temp].crc[1], chunks[temp].crc[2], chunks[temp].crc[3] );
        if ( check_crc( &chunks[temp] ) )
            printf( "[ CRC checked. ]\n" );
        else {
            printf( "[ CRC checking for the %dth chunk failed ...... ]\n", temp+1 );
            exit(1);
        }
    }
    printf( "-----------------------\n%d chunks\n", ret );
    for ( temp=0; temp<ret; temp++ ) {
        printf( "%s ", chunks[temp].type );
    }

    if ( chunks )
        free( chunks );

    free( buf );
}
