/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/drivers/clock_control.h>

int main(void)
{
	uint32_t crt_rate;
	int ret;

	const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ccm_dummy));

	printf("HERE1\n");

	ret = clock_control_get_rate(dev, (void *)2, &crt_rate);
	if (ret != 0) {
		printf("GET RATE RETURNED: %d\n", ret);
		return -EINVAL;
	}

	printf("CURRENT RATE: %u\n", crt_rate);

	ret = clock_control_off(dev, (void *)2);
	if (ret < 0) {
		printf("OFF RETURNED: %d\n", ret);
		return -EINVAL;
	}

	ret = clock_control_set_rate(dev, (void *)2, (void *)(24 * 1000 * 1000));
	if (ret != -EALREADY) {
		printf("SET RATE 1 RETURNED: %d\n", ret);
		return -EINVAL;
	}

	ret = clock_control_get_rate(dev, (void *)2, &crt_rate);
	if (ret != 0) {
		printf("GET RATE 1 RETURNED: %d\n", ret);
		return -EINVAL;
	}

	printf("CURRENT RATE: %u\n", crt_rate);

	ret = clock_control_set_rate(dev, (void *)2, (void *)(12 * 1000 * 1000));
	if (ret < 0) {
		printf("SET RATE 2 RETURNED: %d\n", ret);
		return -EINVAL;
	}

	ret = clock_control_get_rate(dev, (void *)2, &crt_rate);
	if (ret != 0) {
		printf("GET RATE 2 RETURNED: %d\n", ret);
		return -EINVAL;
	}

	printf("CURRENT RATE: %u\n", crt_rate);

	ret = clock_control_set_rate(dev, (void *)2, (void *)(8 * 1000 * 1000));
	if (ret != 8 * 1000 * 1000) {
		printf("SET RATE 3 RETURNED: %d\n", ret);
		return -EINVAL;
	}

	ret = clock_control_get_rate(dev, (void *)2, &crt_rate);
	if (ret != 0) {
		printf("GET RATE 3 RETURNED: %d\n", ret);
		return -EINVAL;
	}

	printf("CURRENT RATE: %u\n", crt_rate);

	ret = clock_control_set_rate(dev, (void *)2, (void *)(1000 * 1000 *
							      1000));
	if (ret != -ENOTSUP) {
		printf("SET RATE 4 RETURNED: %d\n", ret);
		return -EINVAL;
	}

	ret = clock_control_get_rate(dev, (void *)2, &crt_rate);
	if (ret != 0) {
		printf("GET RATE 4 RETURNED: %d\n", ret);
		return -EINVAL;
	}

	printf("CURRENT RATE: %u\n", crt_rate);

	ret = clock_control_set_rate(dev, (void *)2, (void *)(40 * 1000 *
							      1000));
	if (ret < 0) {
		printf("SET RATE 5 RETURNED: %d\n", ret);
		return -EINVAL;
	}

	ret = clock_control_get_rate(dev, (void *)2, &crt_rate);
	if (ret != 0) {
		printf("GET RATE 5 RETURNED: %d\n", ret);
		return -EINVAL;
	}

	printf("CURRENT RATE: %u\n", crt_rate);

	ret = clock_control_set_rate(dev, (void *)2, (void *)(50 * 1000 *
							      1000));
	if (ret < 0) {
		printf("SET RATE 6 RETURNED: %d\n", ret);
		return -EINVAL;
	}

	ret = clock_control_get_rate(dev, (void *)2, &crt_rate);
	if (ret != 0) {
		printf("GET RATE 6 RETURNED: %d\n", ret);
		return -EINVAL;
	}

	printf("CURRENT RATE: %u\n", crt_rate);

	ret = clock_control_set_rate(dev, (void *)2, (void *)(24 * 1000 * 1000));
	if (ret < 0) {
		printf("SET RATE 7 RETURNED: %d\n", ret);
		return -EINVAL;
	}

	ret = clock_control_get_rate(dev, (void *)2, &crt_rate);
	if (ret != 0) {
		printf("GET RATE 7 RETURNED: %d\n", ret);
		return -EINVAL;
	}

	printf("CURRENT RATE: %u\n", crt_rate);

	printf("DONE\n");

	return 0;
}
