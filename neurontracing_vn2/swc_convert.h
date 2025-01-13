#ifndef __SWC_CONVERT_H__
#define __SWC_CONVERT_H__

#include "basic_surf_objs.h"
#include "app2/my_surf_objs.h"

vector<MyMarker*> swc_convert(NeuronTree & nt);
NeuronTree swc_convert(vector<MyMarker*> & inswc);

#endif
