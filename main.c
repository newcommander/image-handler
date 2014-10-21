#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>

extern int read_RGB( char *filename, unsigned char **rgb, int *length, int *width, int *height );
extern int write_RGB( char *filename, unsigned char *rgb, int width, int height );
extern int show_init( char *ip, int port, int *screen_width, int *screen_height );
extern int show_end( int sockfd );
extern int show( int sockfd, unsigned char *buf );

extern char *optarg;

int width = 0, height = 0;
int screen_width = 0, screen_height = 0;

struct Area{
    int x1, y1, x2, y2;
    unsigned char flag;         //one area, one flag, uniquely
    struct Area *next;
};

struct Area_Attribute{
    float h_space;              //RGB to HSV, which H is in range of 0-h_space
    float h_min, h_max;         //range of H, 0 ~ h_space
    float s_min, s_max;         //range of S, 0 ~ 1
    float v_min, v_max;         //range of V, 0 ~ 255
    int range, round;           //for smoothing
    unsigned char threshold;    //after smoothing, it should be set to 0 which is less than threshold, meaning it not in the area, 0 ~ 255
};

struct Pice{
    int width;
    int height;
    unsigned char *buf;
};

void Usage( char *name )
{
    printf( "Usage: %s <-i IP:PORT> [ -t scale ] [ -d hold ] file\n", name );
}

void rect( unsigned char *p, int x1, int y1, int x2, int y2 )
{
    int i, j;

    if( !p ){
        printf( "[rect] No valide data.\n" );
        return;
    }
    if( (x2>=width) || (y2>=height) ){
        printf( "[rect] Cannot drawn a rectagle, out of range.\n" );
        return;
    }
    if( (x1>x2) || (y1>y2) ){
        printf( "[rect] Cannot drawn a rectagle, invalide area.\n" );
        return;
    }

    for( i=1; i<5; i++ ){
        switch(i){
        case 1:
            for( j=1; j<x2-x1+1; j++ ){
                p[ y1*width*3 + (x1+j)*3 + 0 ] = 255;
                p[ y1*width*3 + (x1+j)*3 + 1 ] = 255;
                p[ y1*width*3 + (x1+j)*3 + 2 ] = 0;
            }
            break;
        case 2:
            for( j=1; j<y2-y1+1; j++ ){
                p[ (y1+j)*width*3 + x2*3 + 0 ] = 255;
                p[ (y1+j)*width*3 + x2*3 + 1 ] = 255;
                p[ (y1+j)*width*3 + x2*3 + 2 ] = 0;
            }
            break;
        case 3:
            for( j=0; j<x2-x1; j++ ){
                p[ y2*width*3 + (x1+j)*3 + 0 ] = 255;
                p[ y2*width*3 + (x1+j)*3 + 1 ] = 255;
                p[ y2*width*3 + (x1+j)*3 + 2 ] = 0;
            }
            break;
        case 4:
            for( j=0; j<y2-y1; j++ ){
                p[ (y1+j)*width*3 + x1*3 + 0 ] = 255;
                p[ (y1+j)*width*3 + x1*3 + 1 ] = 255;
                p[ (y1+j)*width*3 + x1*3 + 2 ] = 0;
            }
            break;
        }
    }
}

void bar( int sockfd, int *data, int count, unsigned char r, unsigned char g, unsigned char b )
{
    int i, j, k, n, space, width;
    unsigned char *buf = NULL, *p;
    int len;

    space = 0;
    width = 1;
    len = 0;

    if( !data ){
        printf( "[bar] data is NULL.\n" );
        return;
    }

    for( i=0; i<count; i++ )
        len += width*data[i];

    buf = (unsigned char*)calloc( 4+len*7, sizeof(unsigned char) );
    if( !buf ){
        printf( "[bar]Cannot calloc for show: %s\n", strerror(errno) );
        return;
    }

    buf[0] = len & 0xff;
    buf[1] = ( len >> 8 ) & 0xff;
    buf[2] = ( len >> 16 ) & 0xff;
    buf[3] = ( len >> 24 ) & 0xff;
    
    n = 0;
    for( i=0; i<count; i++ ){
//        if( (i==257) || (i==516) || (i==775) )
//            continue;
        for( j=0; j<width; j++ ){
            for( k=0; k<data[i]; k++ ){
                buf[4+4*n+0] = ( i*(space+width) + j ) & 0xff;
                buf[4+4*n+1] = ( ( i*(space+width) + j ) >> 8 ) & 0xff;
                buf[4+4*n+2] = ( screen_height-k ) & 0xff;
                buf[4+4*n+3] = ( (screen_height-k) >> 8 ) & 0xff;
                n++;
            }
        }
    }
    p = &buf[4+len*4];
    for( i=0; i<len; i++ ){
        p[3*i+0] = r;
        p[3*i+1] = g;
        p[3*i+2] = b;
    }
    show( sockfd, buf );
    free(buf);
}

int* compute( unsigned char *p, int x1, int y1, int x2, int y2 )
{
    int i, j;
    int *d = NULL;

    if( !p ){
        printf( "[compute] No valide data.\n" );
        return NULL;
    }
    if( ( x2>width ) || ( y2>height ) ){
        printf( "[compute] Cannot compute, out of image size.\n" );
        return NULL;
    }

    d = (int*)calloc( 1033, sizeof(int) );
    if( !d ){
        printf( "[compute] Cannot calloc memory for compute.\n" );
        return NULL;
    }

    d[257] = screen_height+1;
    d[516] = screen_height+1;
    d[775] = screen_height+1;

    for( i=y1; i<=y2; i++ ){
        for( j=x1; j<=x2; j++ ){
            d[ p[i*width*3+j*3+0] ] += 1;
            d[ 256+3+p[i*width*3+j*3+1] ] += 1;
            d[ 256+3+256+3+p[i*width*3+j*3+2] ] += 1;
        }
    }

    return d;
}

void edge_old( unsigned char *p, int hold )
{
    unsigned char *wb = NULL;
    int i = 0, j = 0, rgb = 3;

    if( !p ){
        printf( "[edge_old] No valide data.\n" );
        return;
    }
    if( (width==0) || (height==0) )
        return;

    wb = (unsigned char*)calloc( width*height, sizeof(unsigned char) );
    if( !wb ){
        printf( "[edge_rgb] Cannot calloc wb: %s\n", strerror(errno) );
        return;
    }

    //handle the R,G,B channel respectively
    rgb = 3;
    while( rgb-- ){
        unsigned int tt = 0;
        for( i=1; i<height; i++ ){
            for( j=0; j<width; j++ ){

                if( i>2 )
                    tt = ( p[((i-3)*width+j)*3+rgb] + p[((i-2)*width+j)*3+rgb] + p[((i-1)*width+j)*3+rgb] ) / 3;
                else{
                    switch(i){
                    case 1:
                        tt = p[j*3+rgb];
                        break;
                    case 2:
                        tt = ( p[j*3+rgb] + p[(1*width+j)*3+rgb] ) / 2;
                        break;
                    }
                }

                if( abs( p[(i*width+j)*3+rgb] - tt ) > hold )
                    wb[i*width+j] = 255;
            }
        }

        for( i=1; i<width; i++ ){
            for( j=0; j<height; j++ ){

                if( i>2 )
                    tt = ( p[(j*width+i-3)*3+rgb] + p[(j*width+i-2)*3+rgb] + p[(j*width+i-1)*3+rgb] ) / 3;
                else{
                    switch(i){
                    case 1:
                        tt = p[(j*width)*3+rgb];
                        break;
                    case 2:
                        tt = ( p[(j*width)*3+rgb] + p[(j*width+1)*3+rgb] ) / 2;
                        break;
                    }
                }

                if( abs( p[(j*width+i)*3+rgb] - tt ) > hold )
                    wb[j*width+i] = 255;
            }
        }
    }// end while( rgb-- )
    
    for( i=0; i<height; i++ ){
        for( j=0; j<width; j++ ){
            p[(i*width+j)*3+0] = wb[i*width+j];
            p[(i*width+j)*3+1] = wb[i*width+j];
            p[(i*width+j)*3+2] = wb[i*width+j];
        }
    }

    free( wb );
    wb = NULL;
}

int derivative( int *d, int length, int range, int round )
{
    int *d1 = NULL, *d2 = NULL, *p = NULL;
    int i = 0, j = 0;

    if( round<1 ){
        return 0;
    }
    if( !d || length<0 ){
        printf( "[derivative]Invalide source data.\n" );
        return 1;
    }
    if( range<3 )
        range = 3;

    if( !(range%2) )
        range += 1;
    if( range >= length )
        range = (range%2) ? range : range-1;

    d1 = d;
    d2 = (int*)calloc( length, sizeof(int) );
    if( !d2 ){
        printf( "[derivative] Cannot calloc d2: %s\n", strerror(errno) );
        return 1;
    }

    while( round-- )    //which class of derivative
    {
        for( i=0; i<(range-1)/2; i++ ){
            d2[i] = 0;
            j = (range+1)/2;
            while( j-- )
                d2[i] += d1[i+j] - d1[i];
            j = i+1;
            while( j-- )
                d2[i] += d1[i] - d1[i-j];
        }
        for( i=(range-1)/2; i<length-(range-1)/2; i++ ){
            d2[i] = 0;
            j = (range+1)/2;
            while( j-- ){
                d2[i] += d1[i+j] - d1[i];
                d2[i] += d1[i] - d1[i-j];
            }
        }
        for( i=length-(range-1)/2; i<length; i++ ){
            d2[i] = 0;
            j = (range+1)/2;
            while( j-- )
                d2[i] += d1[i] - d1[i-j];
            j = length - i;
            while( j-- )
                d2[i] += d1[i+j] - d[i];
        }

        p = d2;
        d2 = d1;
        d1 = p;
    }

    if( d == d1 )
        free( d2 );
    else{
        for( i=0; i<length; i++ )
            d[i] = d1[i];
        free( d1 );
    }
    d1 = NULL;
    d2 = NULL;
    
    return 0;
}

int* smooth( unsigned char *p, int direc, int n, int range, int round, int rgb )
{
    int *d1 = NULL, *d2 = NULL, *d3 = NULL;
    int i = 0, j = 0, k = 0;
    int line = 0;   //length of scanline

    if( !p || (width==0) || (height==0) ){
        printf( "[smooth] None RGB data.\n" );
        return NULL;
    }
    if( (direc!=0) && (direc!=1) ){
        printf( "[smooth] Invalide scanline direction, 0:transversal, 1:vertical.\n " );
        return NULL;
    }
    if( rgb<0 || rgb>2 ){
        printf( "[smooth] Invalide rgb parameter which must be 0(R) or 1(G) or 2(B)\n" );
        return NULL;
    }

    if( round < 0 )
        round = 0;
    if( range < 3 )
        range = 3;

    d1 = (int*)calloc( ( direc ? height : width ), sizeof(int) );
    d2 = (int*)calloc( ( direc ? height : width ), sizeof(int) );

    if( !d1 || !d2 ){
        printf( "[smooth]Cannot calloc memory fot smooth: %s\n", strerror(errno) );
        free( d1 );
        d1 = NULL;
        free( d2 );
        d2 = NULL;
        return NULL;
    }
    
    if( direc == 0 ){   //scanline is transversal
        line = width;
        for( i=0; i<width; i++ )
            d1[i] = p[(n*width+i)*3+rgb];
    }
    else{               //scanline is vertical
        line = height;
        for( i=0; i<height; i++ )
            d1[i] = p[(i*width+n)*3+rgb];
    }

    if( !(range%2) )    //range must be a strage number
        range += 1;
    if( range >= line )
        range = (range%2) ? range : range-1;
    
    d3 = d1;
    while( round-- )    // how many rounds of smoothing...
    {
        for( j=0; j<(range-1)/2; j++ ){
            d2[j] = 0;
            k = j+(range-1)/2+1;
            while( k-- )
                d2[j] += d1[k];
            d2[j] /= j+(range-1)/2+1;
        }
        for( j=(range-1)/2; j<line-(range-1)/2; j++ ){
            d2[j] = 0;
            for( k=0; k<range; k++ ){
                d2[j] += d1[j-(range-1)/2+k];
            }
            d2[j] /= range;
        }
        for( j=line-(range-1)/2; j<line; j++ ){
            d2[j] = 0;
            k = line-j+(range-1)/2;
            while( k-- )
                d2[j] += d1[line-1-k];
            d2[j] /= line-j+(range-1)/2;
        }

        d3 = d2;
        d2 = d1;
        d1 = d3;
    }
    free( d2 );

    return d1;
}

int edge( unsigned char *p )
{
    unsigned char *wb = NULL, *temp1 = NULL, *temp2 = NULL;
    int *d = NULL, ret = 0, i = 0, j = 0, channel = 3;

    //for smoothing...
    int range = 3;      //core range, >= 3, strage number
    int round = 1;      // >= 1

    //for derivative
    int range1 = 7;     //core range for first class, >= 3, strage number
    int range2 = 5;     //core range for second class, >= 3, strage number

    //for quality control
    int key1 = 80;       //key value for first class derivative,  greater cause quality down, > 0
    int key2 = 100;     //key value for second class derivative, greater cause quality up,   > 0

    if( !p ){
        printf( "[edge] No valide RGB data.\n" );
        ret = 1;
        goto out;
    }
    
    wb = (unsigned char*)calloc( width*height, sizeof(unsigned char) );
    if( !wb ){
        printf( "[main]Cannot calloc for wb: %s\n", strerror(errno) );
        ret = 1;
        goto out;
    }
    
    temp1 = (unsigned char*)calloc( height, sizeof(unsigned char) );
    if( !temp1 ){
        printf( "[edge]Failed to malloc: %s\n", strerror(errno) );
        ret = 1;
        goto out;
    }
    temp2 = (unsigned char*)calloc( width, sizeof(unsigned char) );
    if( !temp2 ){
        printf( "[edge]Failed to malloc: %s\n", strerror(errno) );
        ret = 1;
        goto out;
    }

    channel = 3;
    while( channel-- )
    {
        for( i=0; i<width; i++ )
        {
            memset( temp1, 0, height );
            d = smooth( p, 1, i, range, round, channel );
            if( !d ){
                printf( "[edge]Error when smoothing.\n" );
                ret = 1;
                goto out;
            }

            if( derivative( d, height, range1, 1 ) == 1 ){
                printf( "[edge]Failed to get derivative, vertical, first class\n" );
                ret = 1;
                goto out;
            }
            for( j=0; j<height; j++ )
                if( d[j] < 0 )
                    d[j] = 0-d[j];
            for( j=0; j<height; j++ )
                if( d[j] > key1 )   //the key value for first class derivative
                    temp1[j] = d[j];

            if( derivative( d, height, range2, 1 ) == 1 ){
                printf( "[edge]Failed to get derivative, vertical, second class\n" );
                ret = 1;
                goto out;
            }
            for( j=0; j<height; j++ )
                if( d[j] < 0 )
                    d[j] = 0-d[j];
            for( j=0; j<height; j++ )
                if( temp1[j] != 0 )
                    if( d[j] > key2 ) //the key value for second class derivative
                        temp1[j] = 0;

            for( j=0; j<height; j++ )
                if( temp1[j] != 0 )
                    wb[j*width+i] += temp1[j]/3;
            
            free( d );
            d = NULL;
        }
        
        for( i=0; i<height; i++ )
        {
            memset( temp2, 0, width );
            d = smooth( p, 0, i, range, round, channel );
            if( !d ){
                printf( "[edge]Error when smoothing.\n" );
                ret = 1;
                goto out;
            }

            if( derivative( d, width, range1, 1 ) == 1 ){
                printf( "[edge]Failed to get derivative, transversal, first class\n" );
                ret = 1;
                goto out;
            }
            for( j=0; j<width; j++ )
                if( d[j] < 0 )
                    d[j] = 0-d[j];
            for( j=0; j<width; j++ )
                if( d[j] > key1 )     //the key value for first class derivative
                    temp2[j] = d[j];

            if( derivative( d, width, range2, 1 ) == 1 ){
                printf( "[edge]Failed to get derivative, transversal, second class\n" );
                ret = 1;
                goto out;
            }
            for( j=0; j<width; j++ )
                if( d[j] < 0 )
                    d[j] = 0-d[j];
            for( j=0; j<width; j++ )
                if( temp2[j] != 0 )
                    if( d[j] > key2 ) //the key value for second class derivative
                        temp2[j] = 0;

            for( j=0; j<width; j++ )
                if( temp2[j] != 0 )
                    if( wb[i*width+j] == 0 )
                        wb[i*width+j] += temp2[j]/3;
            
            free( d );
            d = NULL;
        }
    }

    free( temp1 );
    temp1 = NULL;
    free( temp2 );
    temp2 = NULL;

    for( i=0; i<width; i++ ){
        for( j=0; j<height; j++ ){
            p[(j*width+i)*3+0] = wb[j*width+i];
            p[(j*width+i)*3+1] = wb[j*width+i];
            p[(j*width+i)*3+2] = wb[j*width+i];
        }
    }

    free( wb );
    wb = NULL;

    ret = 0;

out:
    if( wb )
        free( wb );
    wb = NULL;
    if( temp1 )
        free( temp1 );
    temp1 = NULL;
    if( temp2 )
        free( temp2 );
    temp2 = NULL;
    if( d )
        free( d );
    d = NULL;
    return ret;
}

void grow( unsigned char *p, int i, int j, unsigned char flag, int *x1, int *y1, int *x2, int *y2 )
{
    // i<height
    // j<width
    if( i == 0 ){
        if( p[(width+j)*3+0] && p[(width+j)*3+1] == 0 ){
            p[(width+j)*3+1] = flag;
            if( i+1 > *y2 )
                *y2 = i+1;
            grow( p, i+1, j, flag, x1, y1, x2, y2 );
        }
        if( j != 0 ){
            if( p[(j-1)*3+0] && p[(j-1)*3+1] ==0 ){
                p[(j-1)*3+1] = flag;
                if( j-1 < *x1 )
                    *x1 = j-1;
                grow( p, i, j-1, flag, x1, y1, x2, y2 );
            }
            if( p[(width+j-1)*3+0] && p[(width+j-1)*3+1] ==0 ){
                p[(width+j-1)*3+1] = flag;
                if( j-1 < *x1 )
                    *x1 = j-1;
                if( i+1 > *y2 )
                    *y2 = i+1;
                grow( p, i+1, j-1, flag, x1, y1, x2, y2 );
            }
        }
        if( j != width-1 ){
            if( p[(j+1)*3+0] && p[(j+1)*3+1] ==0 ){
                p[(j+1)*3+1] = flag;
                if( j+1 > *x2 )
                    *x2 = j+1;
                grow( p, i, j+1, flag, x1, y1, x2, y2 );
            }
            if( p[(width+j+1)*3+0] && p[(width+j+1)*3+1] ==0 ){
                p[(width+j+1)*3+1] = flag;
                if( j+1 > *x2 )
                    *x2 = j+1;
                if( i+1 > *y2 )
                    *y2 = i+1;
                grow( p, i+1, j+1, flag, x1, y1, x2, y2 );
            }
        }
        return;
    }
    if( i == height-1 ){
        if( p[((i-1)*width+j)*3+0] && p[((i-1)*width+j)*3+1] == 0 ){
            p[((i-1)*width+j)*3+1] = flag;
            if( i-1 < *y1 )
                *y1 = i-1;
            grow( p, i-1, j, flag, x1, y1, x2, y2 );
        }
        if( j != 0 ){
            if( p[(i*width+j-1)*3+0] && p[(i*width+j-1)*3+1] ==0 ){
                p[(i*width+j-1)*3+1] = flag;
                if( j-1 < *x1 )
                    *x1 = j-1;
                grow( p, i, j-1, flag, x1, y1, x2, y2 );
            }
            if( p[((i-1)*width+j-1)*3+0] && p[((i-1)*width+j-1)*3+1] == 0 ){
                p[((i-1)*width+j-1)*3+1] = flag;
                if( j-1 < *x1 )
                    *x1 = j-1;
                if( i-1 < *y1 )
                    *y1 = i-1;
                grow( p, i-1, j-1, flag, x1, y1, x2, y2 );
            }
        }
        if( j != width-1 ){
            if( p[(i*width+j+1)*3+0] && p[(i*width+j+1)*3+1] == 0 ){
                p[(i*width+j+1)*3+1] = flag;
                if( j+1 > *x2 )
                    *x2 = j+1;
                grow( p, i, j+1, flag, x1, y1, x2, y2 );
            }
            if( p[((i-1)*width+j+1)*3+0] && p[((i-1)*width+j+1)*3+1] == 0 ){
                p[((i-1)*width+j+1)*3+1] = flag;
                if( j+1 > *x2 )
                    *x2 = j+1;
                if( i-1 < *y1 )
                    *y1 = i-1;
                grow( p, i-1, j+1, flag, x1, y1, x2, y2 );
            }
        }
        return;
    }
    if( j == 0 ){
        if( p[((i-1)*width)*3+0] && p[((i-1)*width)*3+1] == 0 ){
            p[((i-1)*width)*3+1] = flag;
            if( i-1 < *y1 )
                *y1 = i-1;
            grow( p, i-1, j, flag, x1, y1, x2, y2 );
        }
        if( p[((i+1)*width)*3+0] && p[((i+1)*width)*3+1] == 0 ){
            p[((i+1)*width)*3+1] = flag;
            if( i+1 > *y2 )
                *y2 = i+1;
            grow( p, i+1, j, flag, x1, y1, x2, y2 );
        }
        if( p[((i-1)*width+1)*3+0] && p[((i-1)*width+1)*3+1] == 0 ){
            p[((i-1)*width+1)*3+1] = flag;
            if( j+1 > *x2 )
                *x2 = j+1;
            if( i-1 < *y1 )
                *y1 = i-1;
            grow( p, i-1, j+1, flag, x1, y1, x2, y2 );
        }
        if( p[((i+1)*width+1)*3+0] && p[((i+1)*width+1)*3+1] == 0 ){
            p[((i+1)*width+1)*3+1] = flag;
            if( j+1 > *x2 )
                *x2 = j+1;
            if( i+1 > *y2 )
                *y2 = i+1;
            grow( p, i+1, j+1, flag, x1, y1, x2, y2 );
        }
        if( p[(i*width+j+1)*3+0] && p[(i*width+j+1)*3+1] == 0 ){
            p[(i*width+j+1)*3+1] = flag;
            if( j+1 > *x2 )
                *x2 = j+1;
            grow( p, i, j+1, flag, x1, y1, x2, y2 );
        }
        return;
    }
    if( j == width-1 ){
        if( p[((i-1)*width+j)*3+0] && p[((i-1)*width+j)*3+1] == 0 ){
            p[((i-1)*width+j)*3+1] = flag;
            if( i-1 < *y1 )
                *y1 = i-1;
            grow( p, i-1, j, flag, x1, y1, x2, y2 );
        }
        if( p[((i+1)*width+j)*3+0] && p[((i+1)*width+j)*3+1] == 0 ){
            p[((i+1)*width+j)*3+1] = flag;
            if( i+1 > *y2 )
                *y2 = i+1;
            grow( p, i+1, j, flag, x1, y1, x2, y2 );
        }
        if( p[((i-1)*width+j-1)*3+0] && p[((i-1)*width+j-1)*3+1] == 0 ){
            p[((i-1)*width+j-1)*3+1] = flag;
            if( j-1 < *x1 )
                *x1 = j-1;
            if( i-1 < *y1 )
                *y1 = i-1;
            grow( p, i-1, j-1, flag, x1, y1, x2, y2 );
        }
        if( p[((i+1)*width+j-1)*3+0] && p[((i+1)*width+j-1)*3+1] == 0 ){
            p[((i+1)*width+j-1)*3+1] = flag;
            if( j-1 < *x1 )
                *x1 = j-1;
            if( i+1 > *y2 )
                *y2 = i+1;
            grow( p, i+1, j-1, flag, x1, y1, x2, y2 );
        }
        if( p[(i*width+j-1)*3+0] && p[(i*width+j-1)*3+1] == 0 ){
            p[(i*width+j-1)*3+1] = flag;
            if( j-1 < *x1 )
                *x1 = j-1;
            grow( p, i, j-1, flag, x1, y1, x2, y2 );
        }
        return;
    }

    if( p[(i*width+j-1)*3+0] && ( p[(i*width+j-1)*3+1] == 0 ) ){
        p[(i*width+j-1)*3+1] = flag;
        if( j-1 < *x1 )
            *x1 = j-1;
        grow( p, i, j-1, flag, x1, y1, x2, y2 );
    }
    if( p[((i-1)*width+j-1)*3+0] && ( p[((i-1)*width+j-1)*3+1] == 0 ) ){
        p[((i-1)*width+j-1)*3+1] = flag;
        if( j-1 < *x1 )
            *x1 = j-1;
        if( i-1 < *y1 )
            *y1 = i-1;
        grow( p, i-1, j-1, flag, x1, y1, x2, y2 );
    }
    if( p[((i-1)*width+j)*3+0] && ( p[((i-1)*width+j)*3+1] == 0 ) ){
        p[((i-1)*width+j)*3+1] = flag;
        if( i-1 < *y1 )
            *y1 = i-1;
        grow( p, i-1, j, flag, x1, y1, x2, y2 );
    }
    if( p[((i-1)*width+j+1)*3+0] && ( p[((i-1)*width+j+1)*3+1] == 0 ) ){
        p[((i-1)*width+j+1)*3+1] = flag;
        if( j+1 > *x2 )
            *x2 = j+1;
        if( i-1 < *y1 )
            *y1 = i-1;
        grow( p, i-1, j+1, flag, x1, y1, x2, y2 );
    }
    if( p[(i*width+j+1)*3+0] && ( p[(i*width+j+1)*3+1] == 0 ) ){
        p[(i*width+j+1)*3+1] = flag;
        if( j+1 > *x2 )
            *x2 = j+1;
        grow( p, i, j+1, flag, x1, y1, x2, y2 );
    }
    if( p[((i+1)*width+j+1)*3+0] && ( p[((i+1)*width+j+1)*3+1] == 0 ) ){
        p[((i+1)*width+j+1)*3+1] = flag;
        if( j+1 > *x2 )
            *x2 = j+1;
        if( i+1 > *y2 )
            *y2 = i+1;
        grow( p, i+1, j+1, flag, x1, y1, x2, y2 );
    }
    if( p[((i+1)*width+j)*3+0] && ( p[((i+1)*width+j)*3+1] == 0 ) ){
        p[((i+1)*width+j)*3+1] = flag;
        if( i+1 > *y2 )
            *y2 = i+1;
        grow( p, i+1, j, flag, x1, y1, x2, y2 );
    }
    if( p[((i+1)*width+j-1)*3+0] && ( p[((i+1)*width+j-1)*3+1] == 0 ) ){
        p[((i+1)*width+j-1)*3+1] = flag;
        if( j-1 < *x1 )
            *x1 = j-1;
        if( i+1 > *y2 )
            *y2 = i+1;
        grow( p, i+1, j-1, flag, x1, y1, x2, y2 );
    }
}

struct Area* major_area( unsigned char *p, struct Area_Attribute aa )
{
    unsigned char flag = 0;
    int *d = NULL, i = 0, j = 0;
    int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    float *f = NULL, r = 0, g = 0, b = 0, max = 0, min = 0;

    float h_space = 3000;
    float h_min = 0, h_max = 10;
    float s_min = 0, s_max = 1;
    float v_min = 0, v_max = 255;
    int range = 7, round = 4;
    unsigned char threshold = 120;

    struct Area *head = NULL, *tail = NULL;

    h_space = aa.h_space;
    h_min = aa.h_min;
    h_max = aa.h_max;
    s_min = aa.s_min;
    s_max = aa.s_max;
    v_min = aa.v_min;
    v_max = aa.v_max;
    range = aa.range;
    round = aa.round;
    threshold = aa.threshold;

    if( !p ){
        printf( "[major_area] None valid data.\n" );
        return NULL;
    }

    f = (float*)calloc( width*height*3, sizeof(float) );
    if( !f ){
        printf( "[major_area] Cannot calloc for convering RGB to HSI: %s\n", strerror(errno) );
        return NULL;
    }

    for( i=0; i<width*height; i++ ){
        r = (float)p[i*3+0];
        g = (float)p[i*3+1];
        b = (float)p[i*3+2];
        max = r > g ? r : g;
        max = max > b ? max : b;
        min = r < g ? r : g;
        min = min < b ? min : b;

        if( max == min )
            f[i*3+0] = 0;
        else if( max == r )
            f[i*3+0] = h_space*1/6 + (h_space/6) * (g-b) / (max-min);
        else if( max == g )
            f[i*3+0] = h_space*3/6 + (h_space/6) * (b-r) / (max-min);
        else
            f[i*3+0] = h_space*5/6 + (h_space/6) * (r-g) / (max-min);
        
        if( max == min )
            f[i*3+1] = 0;
        else if( (max+min) < 256 )
            f[i*3+1] = (max-min) / (max+min);
        else
            f[i*3+1] = (max-min) / ( 256*2 - (max+min) );
    
        f[i*3+2] = (r+g+b)/3;
    }

    memset( p, 0, width*height*3 );
    for( i=0; i<width*height; i++ ){
        if( f[i*3+0] >= h_min )
        if( f[i*3+0] <= h_max )
        if( f[i*3+1] >= s_min )
        if( f[i*3+1] <= s_max )
        if( f[i*3+2] >= v_min )
        if( f[i*3+2] <= v_max ){
            p[i*3+0] = 255;
            p[i*3+1] = 0;
            p[i*3+2] = 0;
        }
    }

    free( f );
    f = NULL;

    for( i=0; i<width; i++ ){
        d = smooth( p, 1, i, range, round, 0 );
        for( j=0; j<height; j++ ){
            p[(j*width+i)*3+0] = d[j];
        }
        free(d);
        d = NULL;
    }
    for( i=0; i<height; i++ ){
        d = smooth( p, 0, i, range, round, 0 );
        for( j=0; j<width; j++ ){
            p[(i*width+j)*3+0] = d[j];
        }
        free(d);
        d = NULL;
    }

    for( i=0; i<width*height; i++ ){
        if( p[i*3+0] < threshold )
            p[i*3+0] = 0;
    }
    
    for( i=0; i<height; i++ ){
        for( j=0; j<width; j++ ){
            p[(i*width+j)*3+1] = 0;
            p[(i*width+j)*3+2] = 0;
        }
    }

    flag = 0;
    for( i=0; i<height; i++ ){
        for( j=0; j<width; j++ ){
            if( p[(i*width+j)*3+0] && ( p[(i*width+j)*3+1] == 0 ) ){
                x1 = x2 = j;
                y1 = y2 = i;
                flag++;
                p[(i*width+j)*3+1] = flag;
                
                grow( p, i, j, flag, &x1, &y1, &x2, &y2 );
                
                if( !tail )
                    tail = (struct Area*)calloc( 1, sizeof(struct Area) );
                else{
                    tail->next = (struct Area*)calloc( 1, sizeof(struct Area) );
                    tail = tail->next;
                }
                if( !tail ){
                    printf( "[major_area] Failed to calloc for a new Area.\n" );
                    return head;
                }
                if( !head )
                    head = tail;
                tail->x1 = x1;
                tail->y1 = y1;
                tail->x2 = x2;
                tail->y2 = y2;
                tail->flag = flag;
                tail->next = NULL;
            }
        }
    }

    return head;
}

int test( unsigned char *p )
{
    int i = 0, j = 0, i_width = 0, i_height = 0, ret = 0;
    int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    char filename[64];
    unsigned char *p_p = NULL, flag = 0;
    unsigned char *i_edge = NULL, *i_area = NULL, *i_temp = NULL;
    unsigned char *p_edge = NULL, *p_area = NULL, *p_temp = NULL;
    struct Area *a = NULL, *pa = NULL;
    struct Area_Attribute aa;

    if( !p ){
        printf( "[] No valied data.\n" );
        return 1;
    }

    i_edge = (unsigned char*)calloc( width*height*3, sizeof(unsigned char) );
    if( !i_edge ){
        printf( "[] Cannot calloc for generate edge image.\n" );
        ret = 1;
        goto out;
    }
    i_area = (unsigned char*)calloc( width*height*3, sizeof(unsigned char) );
    if( !i_area ){
        printf( "[] Cannot calloc for generate area image.\n" );
        ret = 1;
        goto out;
    }
    i_temp = (unsigned char*)calloc( height+width*height*3*3, sizeof(unsigned char) );
    if( !i_temp ){
        printf( "[] Cannot calloc for temp image.\n" );
        ret = 1;
        goto out;
    }

    memcpy( i_edge, p, width*height*3 );
    if( edge( i_edge ) ){
        printf( "[] Failed to generate edge image.\n" );
        ret = 1;
        goto out;
    }

    aa.h_space = 3000;
    aa.h_min = 1700;
    aa.h_max = 2500;
    aa.s_min = 0;
    aa.s_max = 0.5;
    aa.v_min = 0;
    aa.v_max = 255;
    aa.range = 7;
    aa.round = 4;
    aa.threshold = 120;

    memcpy( i_area, p, width*height*3 );

//    struct timespec ts1, ts2;
//    clock_gettime( CLOCK_PROCESS_CPUTIME_ID, &ts1 );
    a = major_area( i_area, aa );
//    clock_gettime( CLOCK_PROCESS_CPUTIME_ID, &ts2 );
//    printf( "time: %lds, %ldns\n", ts2.tv_sec-ts1.tv_sec, ts2.tv_nsec-ts1.tv_nsec );

    while( a ){
        x1 = a->x1;
        y1 = a->y1;
        x2 = a->x2;
        y2 = a->y2;
        flag = a->flag;
        if( ( (x2-x1) > 10 ) && ( (y2-y1) > 10 ) ){
            p_temp = i_temp;
            i_width = x2 - x1 + 1;
            i_height = y2 - y1 + 1;
            p_p = &p[ ( y1 * width + x1 ) * 3 ];
            p_edge = &i_edge[ ( y1 * width + x1 ) * 3 ];
            p_area = &i_area[ ( y1 * width + x1 ) * 3 ];
            memset( i_temp, 0, i_height+i_width*i_height*3*3 );
            for( i=0; i<i_height; i++ ){
                *p_temp = 0;
                p_temp++;
                for( j=0; j<i_width; j++ ){
                    if( p_area[j*3+1] == flag ){
                        p_temp[j*3+0] = p_p[j*3+0];
                        p_temp[j*3+1] = p_p[j*3+1];
                        p_temp[j*3+2] = p_p[j*3+2];
                        p_temp[(i_width+j)*3+0] = p_edge[j*3+0];
                        p_temp[(i_width+j)*3+1] = p_edge[j*3+1];
                        p_temp[(i_width+j)*3+2] = p_edge[j*3+2];
                        p_temp[(i_width*2+j)*3+0] = p_area[j*3+0];
                        p_temp[(i_width*2+j)*3+1] = p_area[j*3+1];
                        p_temp[(i_width*2+j)*3+2] = p_area[j*3+2];
                    }
                }
                p_temp += i_width*3*3;
                p_p += width*3;
                p_edge += width*3;
                p_area += width*3;
            }
            sprintf( filename, "image_%p.png", a );
            write_RGB( filename, i_temp, i_width*3, i_height );
        }
        else{
            for( i=a->y1; i<=a->y2; i++ ){
                for( j=a->x1; j<=a->x2; j++ ){
                    if( p[(i*width+j)*3+1] == a->flag ){
                        p[(i*width+j)*3+0] = 0;
                        p[(i*width+j)*3+1] = 0;
                        p[(i*width+j)*3+2] = 0;
                    }
                }
            }
        }
        pa = a;
        a = a->next;
        free( pa );
    }

    ret = 0;
out:
    if( i_edge )
        free( i_edge );
    i_edge = NULL;

    if( i_area )
        free( i_area );
    i_area = NULL;

    if( i_temp )
        free( i_temp );
    i_temp = NULL;

    return ret;
}

int main( int argc, char **argv )
{
    int opt = 0, ret = 0;
    unsigned char *rgb = NULL;
    int x0 = 0, y0 = 0;
    int length = 0, image_width = 0, image_height = 0;
    int *d = NULL, scale = 1, hold = 10;

    unsigned char *buf = NULL;
    int i = 0, j = 0;

    int sockfd = 0;
    char addr[25], *t = NULL;
    int port = 19900;
    char filename[512];

    ret = argc;
    while( ret ){
        ret--;
        if( !strncmp( argv[ret], "-i", 2 ) )
            break;
    }
    if( ret == 0 ){
        Usage( argv[0] );
        ret = 1;
        goto out;
    }

    while( ( opt = getopt( argc, argv, "-i:t:d:h" ) ) != -1 ){
        switch( opt ){
        case 'i':
            if( ( strlen( optarg ) ) > 21 ){
                printf( "[main]Bad net address: %s\n", optarg );
                ret = 1;
                goto out;
            }
            t = strchr( optarg, ':' );
            *t = 0;
            t++;
            port = atoi( t );
            if( inet_addr( optarg ) == -1 ){
                printf( "[main]Invalid IP address.\n" );
                ret = 1;
                goto out;
            }
            sprintf( addr, optarg );
            break;
        case 't':
            for( i=0; optarg[i]!=0; i++ ){
                if( !isdigit(optarg[i]) ){
                    printf( "[main]argument -t must be a number.\n" );
                    Usage( argv[0] );
                    ret = 1;
                    goto out;
                }
            }
            scale = atoi( optarg );
            break;
        case 'd':
            for( i=0; optarg[i]!=0; i++ ){
                if( !isdigit(optarg[i]) ){
                    printf( "[main]argument -d must be a number.\n" );
                    Usage( argv[0] );
                    ret = 1;
                    goto out;
                }
            }
            hold = atoi( optarg );
            break;
        case 'h':
            Usage( argv[0] );
            ret = 0;
            goto out;
        case 1:
            sprintf( filename, optarg );
            break;
        case '?':
            Usage( argv[0] );
            ret = 1;
            goto out;
        }
    }

    ret = read_RGB( filename, &rgb, &length, &width, &height );
    if( ret != 0 ){
        printf( "[main]read image failed.\n" );
        ret = 1;
        goto out;
    }

    image_width = width;
    image_height = height;

    sockfd = show_init( addr, port, &screen_width, &screen_height );
    if( sockfd == -1 ){
        printf( "[main]Cannot connect to the viewer: %s:%d\n", addr, port );
        ret = 1;
        goto out;
    }

    if( width > screen_width )
        width = screen_width;
    if( height > screen_height )
        height = screen_height;

    x0 = 0;//800;//800;
    y0 = 0;//1100;//2400;

    if( (x0>image_width) || (y0>image_height) ){
        printf( "[main]The specific area is out of image size: (x0,y0)=(%d,%d)\n", x0, y0 );
        ret = 1;
        goto out;
    }

    if( width+x0 > image_width )
        width = image_width - x0;
    if( height+y0 > image_height )
        height = image_height - y0;

// if the image size is greater than screen_size, uncomment next two line
//    width = image_width;
//    height = image_height;

    {
        buf = (unsigned char*)calloc( height*width*7+4, sizeof(unsigned char) );
        buf[0] = (height*width) & 0xff;
        buf[1] = ( (height*width) >> 8 ) & 0xff;
        buf[2] = ( (height*width) >> 16 ) & 0xff;
        buf[3] = ( (height*width) >> 24 ) & 0xff;
        for( j=0; j<height; j++ ){
            for( i=0; i<width; i++ ){
                buf[ 4+(j*width+i)*4+0 ] = (unsigned char)( i & 0xff );
                buf[ 4+(j*width+i)*4+1 ] = (unsigned char)( (i>>8) & 0xff );
                buf[ 4+(j*width+i)*4+2 ] = (unsigned char)( j & 0xff );
                buf[ 4+(j*width+i)*4+3 ] = (unsigned char)( (j>>8) & 0xff );
            }
        }
        for( i=y0; i<y0+height; i++ )
            memcpy( &buf[ 4 + height*width*4 + (i-y0)*width*3 ], &rgb[(image_width*i+x0)*3], width*3 );
    }
    free( rgb );
    rgb = NULL;

/*
    d = compute( &buf[4+width*height*4], 427, 157, 466, 234 );
    if( !d ){
        printf( "[main]Not statistics data.\n" );
        ret = 1;
        goto out;
    }
    for( i=0; i<1033; i++ )
        d[i] = d[i]/scale;

    d[257] = screen_height+1;
    d[516] = screen_height+1;
    d[775] = screen_height+1;

    bar( sockfd, d, 1033, 0, 255, 0 );
    free( d );
    d = NULL;
*/
    test( &buf[4+width*height*4] );
//    show( sockfd, buf );
    free( buf );
    buf = NULL;

    show_end( sockfd );

    ret = 0;
out:
    if( rgb )
        free( rgb );
    rgb = NULL;

    if( buf )
        free( buf );
    buf = NULL;

    if( d )
        free( d );
    d = NULL;

    return ret;
}
