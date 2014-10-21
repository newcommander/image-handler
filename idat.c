#include "png_editor.h"
#include <zlib.h>


extern unsigned long crc(unsigned char *buf, int len);
extern int chunk_split( unsigned char *buf, int len, Chunk **chunks );
extern int check_crc( Chunk* data );


typedef struct{
	unsigned char r;
	unsigned char g;
	unsigned char b;
} Plte;

int make_plte( unsigned char *rgb1, int hei1, unsigned char *rgb2, int hei2 ){
	int count = 0, i = 0, j = 0, k = 0;
	Plte plte[256];
	unsigned char *p1, *p2;

	p1 = rgb1;
	p2 = rgb2;
	for( i=0; i<hei1; i++ ){
		p1++;
		for( j=0; j<33280; j++ ){
			for( k=0; k<count; k++ ){
				if( ( p1[0]==plte[k].r ) && 
					( p1[1]==plte[k].g ) && 
					( p1[2]==plte[k].b ) ){
					break;
				}
			}
			if( k == count ){
				plte[count].r = p1[0];
				plte[count].g = p1[1];
				plte[count].b = p1[2];
				count++;
				if( count > 256 )
					return 1;
			}
			p1 += 3;
		}
	}
	printf( "count: %d\n", count );
	return 0;
}

/*
 * read filename and stor into *image
 * return file-size on success, or -1 on error
*/
int read_image( char *filename, unsigned char **image ){
	unsigned char *buf, *p;
    int fd, size, temp;
	unsigned char png_signature[8] = 
						{ 0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a };
    struct stat st;
	
	fd = open( filename, O_RDONLY);
    if ( fd == -1 ){
        printf( "open \"%s\" failed: %s.\n", filename, strerror( errno ) );
        return -1;
    }

    if ( fstat( fd, &st ) ){
        printf( "get stat failed: %s.\n", strerror( errno ) );
        close( fd );
        return -1;
    }

    buf = (unsigned char*)calloc( st.st_size, sizeof(unsigned char) );

    if ( !buf ){
		printf( "error when calloc memroy: %s\nExit...", strerror( errno ) );
		close( fd );
        return -1;
    }

    p = buf;
    size = 0;
    while ( size < st.st_size ) {
        size += read( fd, p, st.st_size-size );
        p += size;
    }

    close(fd);
	
	temp = memcmp( png_signature, buf, 8 );
	if( temp ){
		printf( "Invalid png image: %s.\n", filename );
		free( buf );
        printf("free: buf\n");
		*image = NULL;
		return -1;
	}

	*image = buf;
	return size;
}

unsigned int png_index_to_RGB( unsigned char *plte, unsigned char *index, 
										int len, int width, int height,
										unsigned char **out )
{
	int n, i, j;
	unsigned char *p_out = NULL, *p_idat = NULL;

	if( plte == NULL ){
        printf( "[png_index_to_RGB] plte is NULL.\n" );
		return -1;
    }

	if( index == NULL ){
        printf( "[png_index_to_RGB] idat is NULL" );
		return -1;
    }

	n = height*width*3;
	*out = (unsigned char*)calloc( n, sizeof(unsigned char) );
	if( *out == NULL ){
        printf( "[png_index_to_RGB] calloc for out failed: %s.\n", strerror( errno ) );
		return -1;
    }
	
	p_out = *out;
//	p_idat = idat+1;

    for( i=0; i<height; i++ ){
        for( j=0; j<width; j++ ){
            p_out[(i*width+j)*3+0] = plte[index[i*(width+1)+1+j]*3+0];
            p_out[(i*width+j)*3+1] = plte[index[i*(width+1)+1+j]*3+1];
            p_out[(i*width+j)*3+2] = plte[index[i*(width+1)+1+j]*3+2];
        }
    }
    return n;

	while( p_idat <= &(index[len-1]) ){
		if( !((p_idat-index)%(width*3+1)) ){
			*p_out = *p_idat;
			p_out++;
			p_idat++;
		}
		else{
			p_out[0] = plte[(*p_idat)*3+0];
			p_out[1] = plte[(*p_idat)*3+1];
			p_out[2] = plte[(*p_idat)*3+2];
			p_out += 3;
			p_idat++;
		}
		;
	}

	return n;
}


unsigned int idat_decompress( unsigned char *idat, int len_idat, 
									unsigned char *rgb, int len_rgb)
{
 	z_stream z;
 	int ret = 0;

	z.zalloc = NULL;
	z.zfree = NULL;
	z.opaque = (voidpf)0;
	z.next_in = idat;
	z.avail_in = len_idat;
	z.next_out = rgb;
	z.avail_out = len_rgb;

	ret = inflateInit2( &z, 15 );
	if( ret != Z_OK ){
		return 1;
	}

	ret = inflate( &z, Z_NO_FLUSH);
	if( ret != Z_STREAM_END ){
		return 1;
	}

	return 0;
}

unsigned int idat_compress( unsigned char *rgb, int length,
								unsigned char *idat )
{
	z_stream s;
	int ret = 0;
	
	s.zalloc = NULL;
	s.zfree = NULL;
	s.opaque = (voidpf)0;
	s.next_in = rgb;
	s.avail_in = length;
	s.next_out = idat;
	s.avail_out = length-8;

	ret = deflateInit2( &s, 7/*Z_DEFAULT_COMPRESSION*/, Z_DEFLATED,
		15, 8/*MAX_MEM_LEVEL*/, Z_DEFAULT_STRATEGY);
	if( ret != Z_OK ){
		return -1;
	}

	ret = deflate( &s, Z_FINISH);
	if( ret != Z_STREAM_END){
		return -1;
	}

	return (length-8 - s.avail_out);
}

int read_RGB( char *filename, unsigned char **rgb, int *length,
							int *width, int *height )
{
	int ret = 0, temp = 0, i, j;
	int image_len = 0, image_width = 0, image_height = 0, len_idat = 0, len_rgb = 0;
	unsigned char *prgb = NULL, *index_rgb = NULL;
	unsigned char *image = NULL, *idat = NULL, *p = NULL, *q = NULL;
	unsigned char *plte = NULL;

	Chunk *chunks = NULL, *c = NULL, *first_IDAT = NULL;
	int bit_depth = 0, colour_type = 0, num_IDAT = 0;

	*length = 0;
	*height = 0;

	if( filename == NULL ){
        printf( "[read_RGB] invalid file.\n" );
		ret = 1;
		goto out;
	}
	
	image_len = read_image( filename, &image );
	if( (image_len == -1) || ( image == NULL ) ){
        printf( "[read_RGB] read_image failed.\n" );
		ret = 1;
		goto out;
	}

	ret = chunk_split( image, image_len, &chunks );
	if( ret == -1 ){
		printf( "The png image has been destroyed.\n" );
		ret = 1;
		goto out;
	}

	if( memcmp( chunks->type, "IHDR", 4 ) ){
		printf( "Invalid png image, first chunk is not IHDR.\n" );
		ret = 1;
		goto out;
	}
	
    for ( temp=0; temp<ret; temp++ ) {
        if ( !check_crc( &chunks[temp] ) ){
            printf( "CRC checking for the %dth chunk failed.\n", temp+1 );
			//ret = 1;
            //goto out;
        }
    }

	c = chunks;	
	image_width += c->data[0];
	image_width <<= 8;
	image_width += c->data[1];
	image_width <<= 8;
	image_width += c->data[2];
	image_width <<= 8;
	image_width += c->data[3];

	image_height += c->data[4];
	image_height <<= 8;
	image_height += c->data[5];
	image_height <<= 8;
	image_height += c->data[6];
	image_height <<= 8;
	image_height += c->data[7];

	c++;
	first_IDAT = NULL;
	num_IDAT = 0;
	len_idat = 0;
	while( memcmp( c->type, "IEND", 4 ) ){
		if( !memcmp( c->type, "IDAT", 4 ) ){
			first_IDAT = c;
			num_IDAT++;
			len_idat += c->length;
			c++;
			while( !memcmp( c->type, "IDAT", 4 ) ){
				num_IDAT++;
				len_idat += c->length;
				c++;
			}
		}
		else if( !memcmp( c->type, "PLTE", 4 ) ){
			plte = c->data;
			c++;
		}
		else
			c++;
	}

	idat = (unsigned char*)calloc( len_idat, sizeof(unsigned char) );
	if( idat == NULL ){
        printf( "[read_RGB] calloc for idat failed: %s.\n", strerror( errno ) );
		ret = 1;
		goto out;
	}

	c = first_IDAT;
	p = idat;
	for( temp = 0; temp < num_IDAT; temp++ ){
		memcpy( p, c->data, c->length );
		p += c->length;
		c++;
	}

	c = chunks;
	bit_depth = c->data[8];
	colour_type = c->data[9];

/*	if( image != NULL ){
		free( image );
		image = NULL;
	}
*/
	if( bit_depth == 8 ){	//8-bit depth
		if( colour_type == 2){	//RGB image
			
			len_rgb = image_width * image_height * 3 + image_height;
			prgb = (unsigned char*)calloc( len_rgb, sizeof(unsigned char) );
			if( prgb == NULL ){
                printf( "[read_RGB] RGB image calloc failed: %s.\n", strerror( errno ) );
				ret = 1;
				goto out;
			}
			
			ret = idat_decompress( idat, len_idat, prgb, len_rgb);
			if( ret == 1 ){
                printf( "[read_RGB] idat_decompress failed.\n" );
				goto out;
            }
			if( idat != NULL ){
				free( idat );
				idat = NULL;
			}

            p = prgb+1;
            for( i=0; i<image_height; i++ ){
                memcpy( &prgb[image_width*3*i], p, image_width*3 );
                p += image_width*3+1;
            }
		}
		else if( colour_type == 3 ){	//indexed image
			
			temp = image_width * image_height + image_height;
			index_rgb = (unsigned char*)calloc( temp, sizeof(unsigned char) );
			if( index_rgb == NULL ){
                printf( "[read_RGB] indexed image calloc failed: %s.\n", strerror( errno ) );
				ret = 1;
				goto out;
			}

            while( memcmp( c->type, "IEND", 4 ) ){
                if( !memcmp( c->type, "PLTE", 4 ) ){
                    plte = c->data;
                    break;
                }
                c++;
            }

			ret = idat_decompress( idat, len_idat, index_rgb, temp);
			if( ret == 1 ){
                printf( "[read_RGB] idat_decompress failed.\n" );
				goto out;
            }

			if( idat != NULL ){
				free( idat );
				idat = NULL;
			}

			len_rgb = png_index_to_RGB( plte, index_rgb, temp, 
										image_width, image_height, &prgb);

			if( index_rgb != NULL ){
				free( index_rgb );
				index_rgb = NULL;
			}
            
		}
		else if( colour_type == 6 ){	//RGBA image
			len_rgb = image_width * image_height * 4 + image_height;
			prgb = (unsigned char*)calloc( len_rgb, sizeof(unsigned char) );
			if( prgb == NULL ){
                printf( "[read_RGB] RGB image calloc failed: %s.\n", strerror( errno ) );
				ret = 1;
				goto out;
			}
			
            ret = idat_decompress( idat, len_idat, prgb, len_rgb);
			if( ret == 1 ){
                printf( "[read_RGB] idat_decompress failed.\n" );
				goto out;
            }
			if( idat != NULL ){
				free( idat );
				idat = NULL;
			}

            for( i=0; i<image_height; i++ ){
                unsigned char *a, *b, *c;
                int r, ra, rb, rc;
                switch( prgb[i*(image_width*4+1)] ){  //filter type
                case 1:
                    p = &prgb[i*(image_width*4+1)+1];
                    for( j=1; j<image_width; j++ ){
                        p[j*4+0] += p[(j-1)*4+0];
                        p[j*4+1] += p[(j-1)*4+1];
                        p[j*4+2] += p[(j-1)*4+2];
                    }
                    break;
                case 2:
                    p = &prgb[i*(image_width*4+1)+1];
                    q = &prgb[(i-1)*(image_width*4+1)+1];
                    for( j=0; j<image_width; j++ ){
                        p[j*4+0] += q[j*4+0];
                        p[j*4+1] += q[j*4+1];
                        p[j*4+2] += q[j*4+2];
                    }
                    break;
                case 3:
                    p = &prgb[i*(image_width*4+1)+1];
                    q = &prgb[(i-1)*(image_width*4+1)+1];
                    p[0] += q[0]/2;
                    p[1] += q[1]/2;
                    p[2] += q[2]/2;
                    for( j=1; j<image_width; j++ ){
                        p[j*4+0] += (p[(j-1)*4+0]+q[j*4+0])/2;
                        p[j*4+1] += (p[(j-1)*4+1]+q[j*4+1])/2;
                        p[j*4+2] += (p[(j-1)*4+2]+q[j*4+2])/2;
                    }
                    break;
                case 4:
                    p = &prgb[i*(image_width*4+1)+1];
                    a = &prgb[i*(image_width*4+1)+1];
                    b = &prgb[(i-1)*(image_width*4+1)+1];
                    c = &prgb[(i-1)*(image_width*4+1)+1];
                    
                    r = b[0];
                    ra = r;
                    rb = 0;
                    rc = r;
                    if( r == 0 )
                        p[0] = a[0];
                    else
                        p[0] = 0;
                    r = b[1];
                    ra = r;
                    rb = 0;
                    rc = r;
                    if( r == 0 )
                        p[1] = a[1];
                    else
                        p[1] = 0;
                    r = b[2];
                    ra = r;
                    rb = 0;
                    rc = r;
                    if( r == 0 )
                        p[2] = a[2];
                    else
                        p[2] = 0;
                    
                    b += 5;
                    p += 5;
                    for( j=1; j<image_width; j++ ){
                        r = a[0] + b[0] - c[0];
                        ra = abs( r - a[0] );
                        rb = abs( r - b[0] );
                        rc = abs( r - c[0] );
                        if( (ra<=rb) && (ra<=rc) )
                            p[j*4+0] += a[0];
                        else if( rb <= rc )
                            p[j*4+0] += b[0];
                        else
                            p[j*4+0] += c[0];
                        r = a[1] + b[1] - c[1];
                        ra = abs( r - a[1] );
                        rb = abs( r - b[1] );
                        rc = abs( r - c[1] );
                        if( (ra<=rb) && (ra<=rc) )
                            p[j*4+1] += a[1];
                        else if( rb <= rc )
                            p[j*4+1] += b[1];
                        else
                            p[j*4+1] += c[1];
                        r = a[2] + b[2] - c[2];
                        ra = abs( r - a[2] );
                        rb = abs( r - b[2] );
                        rc = abs( r - c[2] );
                        if( (ra<=rb) && (ra<=rc) )
                            p[j*4+2] += a[2];
                        else if( rb <= rc )
                            p[j*4+2] += b[2];
                        else
                            p[j*4+2] += c[2];
                        a += 4;
                        b += 4;
                        c += 4;
                    }
                    break;
                }
            }

            //cut down the alpha channle
            for( i=0; i<image_height; i++ ){
                for( j=0; j<image_width; j++ ){
                    prgb[(i*image_width+j)*3+0] = prgb[(i*image_width+j)*4+i+1+0];
                    prgb[(i*image_width+j)*3+1] = prgb[(i*image_width+j)*4+i+1+1];
                    prgb[(i*image_width+j)*3+2] = prgb[(i*image_width+j)*4+i+1+2];
                }
            }
        }
		else{
			printf( "Only support 8-bit/RGB,RGBA,index image.\n" );
			ret = 1;
			goto out;
		}
	}
	else{
        printf( "Only support 8-bit/RGB,RGBA,index image.\n" );
		ret = 1;
		goto out;
	}

	*rgb = prgb;
	*length = len_rgb;
	*width = image_width;
	*height = image_height;
	ret = 0;

out:
	if( index_rgb != NULL ){
		free( index_rgb );
		index_rgb = NULL;
	}

	if( idat != NULL ){
		free( idat );
		idat = NULL;
	}
	
	if( chunks != NULL){
		free( chunks );
		chunks = NULL;
	}
	
	if( image != NULL ){
		free( image );
		image = NULL;
	}

	return ret;
}

int write_IHDR( int fd, int width, int height )
{
	int c;
	unsigned char buf[33], *p;
	unsigned char png_signature[8] = 
						{ 0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a };

	memset( buf, 0, 33 );

	memcpy( buf, png_signature, 8);	

	buf[8] = 0;	//chunk length
	buf[9] = 0;
	buf[10] = 0; 
	buf[11] = 13;

	buf[12] = 'I';	//chunk type
	buf[13] = 'H';
	buf[14] = 'D';
	buf[15] = 'R';

	p = (unsigned char*)&width;
	buf[16] = p[3];
	buf[17] = p[2];
	buf[18] = p[1];
	buf[19] = p[0];

	p = (unsigned char*)&height;
	buf[20] = p[3];
	buf[21] = p[2];
	buf[22] = p[1];
	buf[23] = p[0];

	buf[24] = 8;		//bit depth
	buf[25] = 2;		//colour type
	buf[26] = 0;		//compression method
	buf[27] = 0;		//filter method
	buf[28] = 0;		//interlace method

	c = 0;
	c = crc( &(buf[12]), 17 );
	buf[29] = ( c >> 24 ) & 0x000000ff;
	buf[30] = ( c >> 16 ) & 0x000000ff;
	buf[31] = ( c >> 8 ) & 0x000000ff;
	buf[32] = c & 0x000000ff;

	return write( fd, buf, 33 );
}

int write_IDAT( int fd, unsigned char *rgb, int length )
{
	int ret = 0, c = 0;
	unsigned char *idat = NULL;

	idat = (unsigned char*)calloc( length, sizeof(unsigned char) );
	if( idat == NULL ){
		printf( "[write_IDAT] failed calloc: %s\n", strerror( errno ) );
		return -1;
	}
	ret = idat_compress( rgb, length, &(idat[8]));
	if( ret == -1 ){
		printf( "[write_IDAT] idat compression failed.\n" );
		return -1;
	}

	idat[0] = (unsigned char)( ( ret >> 24 ) & 0x000000ff );
	idat[1] = (unsigned char)( ( ret >> 16 ) & 0x000000ff );
	idat[2] = (unsigned char)( ( ret >> 8 ) & 0x000000ff );
	idat[3] = (unsigned char)( ret & 0x000000ff );

	idat[4] = 'I';
	idat[5] = 'D';
	idat[6] = 'A';
	idat[7] = 'T';

	c = crc( &(idat[4]), ret+4 );
	idat[ret+8] = (unsigned char)( ( c >> 24 ) & 0x000000ff );
	idat[ret+9] = (unsigned char)( ( c >> 16 ) & 0x000000ff );
	idat[ret+10] = (unsigned char)( ( c >> 8 ) & 0x000000ff );
	idat[ret+11] = (unsigned char)( c & 0x000000ff );

	ret = write( fd, idat, ret+12 );
	
	free( idat );
	return ret;
}

int write_IEND( int fd )
{
	unsigned char buf[12];

	//IEND chunk
	buf[0] = 0;	
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 0;
	buf[4] = 'I';
	buf[5] = 'E';
	buf[6] = 'N';
	buf[7] = 'D';
	buf[8] = 0xae;
	buf[9] = 0x42;
	buf[10] = 0x60;
	buf[11] = 0x82;

	return write( fd, buf, 12 );
}

int write_RGB( char *filename, unsigned char *rgb, int width, int height )
{
	int fd = 0;
	
	fd = open( filename, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH );
	if( fd == -1 ){
		printf( "Open file \"%s\" failed: %s.\n", filename, strerror( errno ) );
		return -1;
	}

	write_IHDR( fd, width, height );

	write_IDAT( fd, rgb, height*width*3+height );

	write_IEND( fd );

	close( fd );
	return 0;
}

int main_idat( int argc, char **argv )
{
	int ret = 0, i = 0, offset = 0;
	int length1 = 0, width1 = 0, height1 = 0;
    int length2 = 0, width2 = 0, height2 = 0;

	unsigned char *rgb1 = NULL, *p1 = NULL;
	unsigned char *rgb2 = NULL, *p2 = NULL;

	if( argc < 5 ){
		printf( "Usage: %s source1 source2 offset target\n", argv[0] );
		return 1;
	}
//////////////////////////////////////////////////////////////////////////////////////////
    unsigned char *lala;
    lala = (unsigned char*)calloc(600*400*3+400, 1);
    write_RGB( "6.png", lala, 600, 400 );
    free(lala);
    return 0;
//////////////////////////////////////////////////////////////////////////////////////////
	ret = read_RGB( argv[1], &rgb1, &length1, &width1, &height1 );
	if( ret != 0 ){
        printf( "[main] read_RGB failed for: %s.\n", argv[1] );
		ret = 1;
		goto out;
	}
goto save;
//	ret = read_RGB( argv[2], &rgb2, &length2, &width2, &height2 );
//	if( ret != 0 ){
//        printf( "[main] read_RGB failed for: %s.\n", argv[2] );
//		ret = 1;
//		goto out;
//	}
	
    rgb1 = (unsigned char*)calloc( length1+length2, 1 );
    if( rgb1 == NULL ){
        printf( "failed length1+length2\n" );
        goto out;
    }
	
    if( width1 != width2){
        printf( "[main] width1 != width2.\n" );
		ret = 1;
		goto out;
	}

	offset = atoi( argv[3] );

	if( (rgb1 == NULL) || (rgb2 == NULL) ){
        printf( "[main] rgb1 or rgb2 invalied.\n" );
		ret  = 1;
		goto out;
	}

    p1 = &rgb1[length1-offset*(width1*3+1)];
    p2 = rgb2;
	for( i=1; i<=height2; i++ ){
		memcpy( p1, p2, width2*3+1 );
		p1 += width2*3+1;
        p2 += width2*3+1;
	}
	free( rgb2 );
	rgb2 = NULL;

    //////////////////////////////////////////////////////////////////////////////////////////

    int ff  = 0;
save:
    ff = open( argv[4], O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH );
	if( ff == -1 ){
		printf( "Open file \"%s\" failed: %s.\n", argv[4], strerror( errno ) );
		return -1;
	}
    unsigned char buf[1280000];

	write_IHDR( ff, width1, height1+14253-181 );

    buf[0] = 0x0b;
    buf[1] = 0xef;
    buf[2] = 0xc9;
    buf[3] = 0x3f;
    buf[4] = 'I';
    buf[5] = 'D';
    buf[6] = 'A';
    buf[7] = 'T';
    write( ff, buf, 8 );

    z_stream s;

    s.zalloc = NULL;
    s.zfree = NULL;
    s.opaque = (voidpf)0;
    s.next_in = rgb1;
    s.avail_in = length1+33280*(14253-181)*3+14253-181;
    s.next_out = buf;
    s.avail_out = 1280000;

    ret = deflateInit2( &s, 7, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY );
    if( ret != Z_OK ){
        printf( "IDAT compression failed.\n" );
        goto out;
    }

	int fd = open( "temp.data", O_RDONLY );
    read( fd, &(rgb1[length1-33280*(121)*3-121]), 33280*121*3+121 );
    int www = length1, last_avail_in = s.avail_in;
    unsigned int need = 1, rest = 1423033773;
    i = 0;
    while( s.avail_in != 0 ){
        ret = deflate( &s, Z_FINISH);
        if( ret != Z_STREAM_END ){
            i += 1280000 - s.avail_out;
            write( ff, buf, 1280000-s.avail_out );
            printf( "avail_in: %u\tavail_out: %d\n", s.avail_in, s.avail_out );

            www -= last_avail_in - s.avail_in;
            last_avail_in = s.avail_in;
            if( www < 300000000 ){
                if( need ){
                    memcpy( rgb1, s.next_in, www );
                    rest -= read( fd, &(rgb1[www]), length1-www );
                    s.next_in = rgb1;
                    printf( "\t\t\t\t\trest: %u\n", rest );
                    if( rest <= 0 )
                        need = 0;
                }
                www = length1;
            }

            s.next_out = buf;
            s.avail_out = 1280000;
        }
        else{
            i += 1280000 - s.avail_out;
            write( ff, buf, 1280000-s.avail_out );
            printf( "[ok]\n" );
        }
    }
    close( fd );
    printf( "Total idat: %d\n", i );

    buf[0] = 'c';
    buf[0] = 'r';
    buf[0] = 'c';
    buf[0] = 'r';
    write( ff, buf, 4 );

	write_IEND( ff );

	close( ff );
    //////////////////////////////////////////////////////////////////////////////////////////
//	ret = write_RGB( argv[4], &(rgb1[(width1*3+1)*3]), width1, height1-3 );

out:
	if( rgb1 != NULL ){
		free( rgb1 );
		rgb1 = NULL;
	}

	if( rgb2 != NULL ){
		free( rgb2 );
		rgb2 = NULL;
	}

	return ret;
}
