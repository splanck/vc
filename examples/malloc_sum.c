#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int n = 5;
    int *nums = malloc(n * sizeof(int));
    if (!nums) {
        perror("malloc");
        return 1;
    }
    for (int i = 0; i < n; i++)
        nums[i] = i + 1;

    int sum = 0;
    for (int i = 0; i < n; i++)
        sum += nums[i];

    printf("sum = %d\n", sum);
    free(nums);
    return 0;
}
