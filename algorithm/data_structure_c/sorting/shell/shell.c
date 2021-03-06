#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAX_NUM		128

static int _create_random_number(int *pnum);
static int _print_array(const int *pnum);
static int _insertion_sort(int *pnum, int size);
static int _shell_sort(int *pnum, int size);


static int _create_random_number(int *pnum)
{
	int i;

	srand(time(NULL));
	for (i = 0; i < MAX_NUM; i++)
		pnum[i] = rand() % 1000;

	return 0;
}

static int _print_array(const int *pnum)
{
	int i;

	for (i = 0; i < MAX_NUM; i++)
	{
		printf("%3d\t", pnum[i]);

		if ((i + 1) % 10 == 0)
			printf("\n");
	}
	printf("\n");

	return 0;
}

static int _insertion_sort(int *pnum, int size)
{
	int i;
	int j;
	int temp;

	for (i = 0; i < size; i++)
	{
		for (j = i + 1; j; j--)
		{
			if (pnum[j - 1] > pnum[j])
			{
				temp = pnum[j];
				pnum[j] = pnum[j - 1];
				pnum[j - 1] = temp;
			}
		}
	}

	return 0;
}

static int _shell_sort(int *pnum, int size)
{
	int i;

	if (size > 2)
		_shell_sort(pnum, size >> 1);

	for (i = 0; i < MAX_NUM; i += size)
	{
		if (_insertion_sort(&pnum[i], size) < 0)
			goto error;
	}

	return 0;

error :
	return -1;
}

int main(void)
{
	int num[MAX_NUM] = {0, };

	if (_create_random_number(num) < 0)
		goto error;

	printf("\n----------------- before ----------------\n\n");
	if (_print_array(num) < 0)
		goto error;

	// shell sort
	if (_shell_sort(num, MAX_NUM) < 0)
		goto error;

	printf("\n----------------- after ----------------\n\n");
	if (_print_array(num) < 0)
		goto error;

	return 0;

error :
	return -1;
}
