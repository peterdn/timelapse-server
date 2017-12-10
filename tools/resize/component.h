#ifndef __OMX_HELPER_COMPONENT_H__
#define __OMX_HELPER_COMPONENT_H__

#include <ilclient.h>
#include <stdio.h>


struct OMX_HELPER_COMPONENT_T;

typedef struct OMX_HELPER_COMPONENT_T {
  ILCLIENT_T *ilclient_handle;
  COMPONENT_T *component;
  char *component_name;

  int inport;
  int outport;
  struct OMX_HELPER_COMPONENT_T *next_component;
} OMX_HELPER_COMPONENT;


int create_and_initialise_image_decode_component(
    ILCLIENT_T *ilclient_handle, OMX_HELPER_COMPONENT *component);

int create_and_initialise_image_encode_component(
    ILCLIENT_T *ilclient_handle, OMX_HELPER_COMPONENT *component);

int create_and_initialise_resize_component(
    ILCLIENT_T *ilclient_handle, OMX_HELPER_COMPONENT *component);

OMX_HELPER_COMPONENT * connect_components(OMX_HELPER_COMPONENT *input_component,
    OMX_HELPER_COMPONENT *output_component);

int initialise_pipeline(OMX_HELPER_COMPONENT *input_component);

void set_component_input_image_format(OMX_HELPER_COMPONENT *component,
    OMX_IMAGE_CODINGTYPE image_format);

void set_component_output_image_format(OMX_HELPER_COMPONENT *component,
    OMX_IMAGE_CODINGTYPE image_format);

int set_resize_params(OMX_HELPER_COMPONENT *component,
    int width, int height);

int start_pipeline(OMX_HELPER_COMPONENT *input_component);

int prime_component_with_stream(OMX_HELPER_COMPONENT *component, FILE *f);


#endif // #ifndef __OMX_HELPER_COMPONENT_H__
