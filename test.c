#include <stdio.h>
#include <string.h>
#include <math.h>

extern int show_init( char *ip, int port );
extern int show_end( int sockfd );
extern int show( int sockfd, unsigned char *buf );

double Get_e()
{
	int c = 1, i = 34, j = 0;  //i,循环次数，控制取值精度
	double e = 0, t = 0;
	while(i--)
	{
		j = i;
		c = 1;
		while(j)
		{
			c *= j;
			j--;
		}
		t = 1 / (double)c;
		e += t;
	}
	return e;
}
int main()
{
	double e, x, x2, dx, exp, range, t, pai;  //由泊松积分公式计算圆周率（精度有限）
	range = 5;
	dx = 0.001;
	x = range * (-1);
	pai = 0;
	e = Get_e();
	while(x <= range)
	{
		x2 = x * x;
		exp = x2 * (-1);
		t = pow(e, exp);
		t *= dx;
		pai += t;
		x += dx;
	}
	pai *= pai;

    unsigned char buf[1360*7+4];
    memset( buf, 0, 1360*7+4 );
    int count = 1360;
    buf[0] = count & 0xff;
    buf[1] = ( count >> 8 ) & 0xff;
    buf[2] = ( count >> 16 ) & 0xff;
    buf[3] = ( count >> 24 ) & 0xff;
    int i, j;
    for( i=0; i<count; i++ ){
        j = (int)( 1400* pow( e, ((((double)(i-680))/200)*(((double)(i-680))/200))/(-2) ) / pow( 2*pai, 0.5 ) );
        buf[4+i*4+0] = (unsigned char)( i & 0xff );
        buf[4+i*4+1] = (unsigned char)( (i>>8) & 0xff );
        buf[4+i*4+2] = (unsigned char)( (737-j) & 0xff );
        buf[4+i*4+3] = (unsigned char)( ((737-j)>>8) & 0xff );
        buf[4+count*4+i*3+4] = 0;
        buf[4+count*4+i*3+5] = 255;
        buf[4+count*4+i*3+6] = 0;
    }

    int sockfd = show_init( "192.168.1.12", 19900 );
    show( sockfd, buf );
    show_end( sockfd );
	return 0;
}
