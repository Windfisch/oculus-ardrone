/*
 * OpenHMD - Free and Open Source API and drivers for immersive technology.
 * Copyright (C) 2013 Fredrik Hultin.
 * Copyright (C) 2013 Jakob Bornecrantz.
 * Distributed under the Boost 1.0 licence, see LICENSE for full text.
 */

/* Simple Test */

#include <openhmd.h>
#include <stdio.h>

void ohmd_sleep(double);


int init_ohmd(ohmd_context** ctx_, ohmd_device** hmd_)
{
	ohmd_context* ctx = ohmd_ctx_create();

	// Probe
	int num_devices = ohmd_ctx_probe(ctx);
	if(num_devices < 0)
	{
		printf("device probing failed: %s\n", ohmd_ctx_get_error(ctx));
		return -1;
	}

	printf("i've got %d devices\n\n", num_devices);

	ohmd_device* hmd = ohmd_list_open_device(ctx, 0);
	
	if(!hmd)
	{
		printf("open failed! %s\n", ohmd_ctx_get_error(ctx));
		return -1;
	}

	// Print hardware information for the opened device
	int ivals[2];
	ohmd_device_geti(hmd, OHMD_SCREEN_HORIZONTAL_RESOLUTION, ivals);
	ohmd_device_geti(hmd, OHMD_SCREEN_VERTICAL_RESOLUTION, ivals + 1);
	printf("resolution:         %i x %i\n", ivals[0], ivals[1]);

	*ctx_=ctx;
	*hmd_=hmd;
}

int main(int argc, char** argv)
{
	ohmd_context* ctx;
	ohmd_device* hmd;

	init_ohmd(&ctx,&hmd);

	// Ask for n rotation quaternions
	for(int i = 0; i < 10000; i++){
		ohmd_ctx_update(ctx);

		float quat[4];
		ohmd_device_getf(hmd, OHMD_ROTATION_QUAT, quat);
		printf("quat:\t %f\t%f\t%f\t%f\n", quat[0],quat[1],quat[2],quat[3]);


		ohmd_sleep(.01);
	}

	ohmd_ctx_destroy(ctx);
	
	return 0;
}


