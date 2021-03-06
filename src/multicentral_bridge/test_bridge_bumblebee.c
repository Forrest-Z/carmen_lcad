 /*********************************************************
 *
Got * This source code is part of the Carnegie Mellon Robot
 * Navigation Toolkit (CARMEN)
 *
 * CARMEN Copyright (c) 2002 Michael Montemerlo, Nicholas
 * Roy, Sebastian Thrun, Dirk Haehnel, Cyrill Stachniss,
 * and Jared Glover
 *
 * CARMEN is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public 
 * License as published by the Free Software Foundation; 
 * either version 2 of the License, or (at your option)
 * any later version.
 *
 * CARMEN is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied 
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more 
 * details.
 *
 * You should have received a copy of the GNU General 
 * Public License along with CARMEN; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, 
 * Suite 330, Boston, MA  02111-1307 USA
 *
 ********************************************************/

#include <carmen/carmen.h>
#include <carmen/global.h>
#include <carmen/param_interface.h>
#include <carmen/multicentral.h>
#include <carmen/localize_ackerman_interface.h>
#include <carmen/bumblebee_basic_interface.h>
#include <carmen/bumblebee_basic_messages.h>
#include <carmen/rddf_interface.h>
#include <carmen/rddf_messages.h>


static IPC_CONTEXT_PTR car02_context;
static IPC_CONTEXT_PTR car01_context;
static IPC_CONTEXT_PTR current_context;

int camera = 3;


void
publish_rddf_annotation_copy(carmen_rddf_annotation_message *rddf_annotation_message)
{
	IPC_RETURN_TYPE err;

	err = IPC_publishData(CARMEN_RDDF_ANNOTATION_MESSAGE_NAME, rddf_annotation_message);
	carmen_test_ipc_exit(err, "Could not publish", CARMEN_RDDF_ANNOTATION_MESSAGE_NAME);
}


void
carmen_bumblebee_basic_stereoimage_message_handler(carmen_bumblebee_basic_stereoimage_message *stereo_image)
{
//	printf("Entrei no Handler da bumblebee\n");
	if (IPC_getContext() != car01_context)
			return;
//	printf("To no contexto certo\n");
//
//	static double total_time1 = 0.0;
//	static int vezes1 = 0;
//	if (vezes1 == 100)
//	{
//		printf("BUMBLEBEE A %lf hz!!\n",(vezes1/(carmen_get_time() - total_time1)));
//		printf("Host %s\n",stereo_image->host);
//		total_time1 = carmen_get_time();
//		vezes1 = 0;
//	}
//	else
//		vezes1++;

	IPC_RETURN_TYPE ipc_erro_msg;
	ipc_erro_msg = IPC_setContext(car02_context);
	printf("IPC_erro: %d\n", ipc_erro_msg);
	ipc_erro_msg = carmen_bumblebee_basic_publish_message(3, stereo_image);
//	printf("IPC_erro Bumblebee: %d\n", ipc_erro_msg);
//	printf("Publiquei\n");
	ipc_erro_msg = IPC_setContext(car01_context);
//	printf("IPC_erro2: %d\n", ipc_erro_msg);
}


void
carmen_localize_ackerman_globalpos_message_handler(carmen_localize_ackerman_globalpos_message *msg)
{
	if (IPC_getContext() != car01_context)
		return;

	static double total_time = 0.0;
	static int vezes = 0;
	if (vezes == 100)
	{
		printf("TO RECEBENDO O LOCALIZE TBM!! A %lf hz !!\n",(vezes/(carmen_get_time()-total_time)));
		printf("Host %s\n", msg->host);
		total_time = carmen_get_time();
		vezes = 0;
	}
	else
		vezes++;

	IPC_setContext(car02_context);
	carmen_localize_ackerman_publish_globalpos_message(msg);
	IPC_setContext(car01_context);
}


void
carmen_rddf_annotation_message_handler(carmen_rddf_annotation_message *msg)
{
	if (IPC_getContext() != car01_context)
		return;

	IPC_setContext(car02_context);
	publish_rddf_annotation_copy(msg);
	IPC_setContext(car01_context);
}


void test_subscribe_messages(void)
{
	carmen_bumblebee_basic_subscribe_stereoimage(3, NULL,
			(carmen_handler_t) carmen_bumblebee_basic_stereoimage_message_handler,
			CARMEN_SUBSCRIBE_LATEST);
	carmen_localize_ackerman_subscribe_globalpos_message(NULL,
			(carmen_handler_t) carmen_localize_ackerman_globalpos_message_handler,
			CARMEN_SUBSCRIBE_LATEST);
	 carmen_rddf_subscribe_annotation_message(NULL,
			(carmen_handler_t) carmen_rddf_annotation_message_handler, CARMEN_SUBSCRIBE_LATEST);


}


void test_ipc_exit_handler(void)
{
  fprintf(stderr, "Central died.\n");
}


void
register_message()
{
	carmen_bumblebee_basic_define_messages(3);
	carmen_localize_ackerman_define_globalpos_messages();
	carmen_rddf_define_messages();
}


int main(int argc, char **argv)
{
  carmen_centrallist_p centrallist;

  /* set this if it is OK for the program to run without connections
     to any centrals */
  carmen_multicentral_allow_zero_centrals(1);

  /* connect to all IPC servers */
  centrallist = carmen_multicentral_initialize(argc, argv, 
					       test_ipc_exit_handler);

  /* start thread that monitors connections to centrals */
  carmen_multicentral_start_central_check(centrallist);

  /* subscribe to messages from each central */
  carmen_multicentral_subscribe_messages(centrallist, 
					 test_subscribe_messages);
  if (centrallist->num_centrals > 0)
  {
	  if (strcmp(centrallist->central[0].host, "car02") == 0)
	  {
		  car02_context = centrallist->central[0].context;
		  car01_context = centrallist->central[1].context;
	  }
	  else
	  {
		  car02_context = centrallist->central[1].context;
		  car01_context = centrallist->central[0].context;
	  }
	  current_context = IPC_getContext();
	  IPC_setContext(car02_context);
	  register_message();
	  IPC_setContext(current_context);

  }

  do {
    /* handle IPC messages across all centrals */
    carmen_multicentral_ipc_sleep(centrallist, 0.1);

    /* attempt to reconnect any missing centrals */
    carmen_multicentral_reconnect_centrals(centrallist, NULL,
					   test_subscribe_messages);
  } while(1);
  return 0;
}
