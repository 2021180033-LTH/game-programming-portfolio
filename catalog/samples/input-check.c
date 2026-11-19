#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
int main(void)
{
	double x;
	printf("실수를 입력하시오: ");
	if (scanf("%lf", &x)!= 1)
	{
		printf("실수를 입력하지 않았습니다.");
		return 0;
	}
	double y;
	printf("실수를 입력하시오: ");
	if (scanf("%lf", &y) != 1)
	{
		printf("실수를 입력하지 않았습니다.");
		return 0;
	}
	double z;
	printf("실수를 입력하시오: ");
	if (scanf("%lf", &z) != 1)
	{
		printf("실수를 입력하지 않았습니다.");
		return 0;
	}
	double sum = x + y + z, avg = sum / 3.0;
	printf("합은 %lf이고 평균은 %lf입니다.", sum, avg);
	return 0;
}
