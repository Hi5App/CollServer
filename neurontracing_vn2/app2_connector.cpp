//last change: by Hanchuan Peng. 2012-12-30, 2013-06-05

#include "vn_app2.h"
#include "app2/fastmarching_tree.h"
#include "app2/fastmarching_dt.h"
#include "app2/hierarchy_prune.h"
#include "app2/marker_radius.h"
#include "swc_convert.h"
#include "vn_imgpreprocess.h"
#include "volimg_proc.h"

bool saveSWC_file_app2(string swc_file, vector<MyMarker*> & outmarkers, list<string> & infostring)
{
    if(swc_file.find_last_of(".dot") == swc_file.size() - 1) return saveDot_file(swc_file, outmarkers);

    cout<<"marker num = "<<outmarkers.size()<<", save swc file to "<<swc_file<<endl;
    map<MyMarker*, int> ind;
    ofstream ofs(swc_file.c_str());

    if(ofs.fail())
    {
        cout<<"open swc file error"<<endl;
        return false;
    }
    ofs<<"#name "<<"APP2_Tracing"<<endl;
    ofs<<"#comment "<<endl;

    list<string>::iterator it;
    for (it=infostring.begin();it!=infostring.end(); it++)
        ofs<< *it <<endl;

    ofs<<"##n,type,x,y,z,radius,parent"<<endl;
    for(int i = 0; i < outmarkers.size(); i++) ind[outmarkers[i]] = i+1;

    for(int i = 0; i < outmarkers.size(); i++)
    {
        MyMarker * marker = outmarkers[i];
        int parent_id;
        if(marker->parent == 0) parent_id = -1;
        else parent_id = ind[marker->parent];
        ofs<<i+1<<" "<<marker->type<<" "<<marker->x<<" "<<marker->y<<" "<<marker->z<<" "<<marker->radius<<" "<<parent_id<<endl;
    }
    ofs.close();
    return true;
}

bool proc_app2(PARA_APP2 &p)
{
    QString infile = p.inimg_file;
    p.p4dImage = new Image4DSimple;
    p.p4dImage->loadImage((char *)(qPrintable(infile)), false);
//    p.p4dImage = callback.loadImage((char *)(qPrintable(infile) ));
    if (!p.p4dImage || !p.p4dImage->valid()) return false;
    else
    {
        p.xc0 = p.yc0 = p.zc0 = 0;
        p.xc1 = p.p4dImage->getXDim()-1;
        p.yc1 = p.p4dImage->getYDim()-1;
        p.zc1 = p.p4dImage->getZDim()-1;
    }

    int i;
    list<string>::iterator it;
    v3d_msg("start to preprocessing.\n", 0);
    
    QElapsedTimer timer1;
    timer1.start();
    
    //copy data to a temp buffer, so that won't influence the original input data
    Image4DSimple * p4dImageNew = 0;
    
    if (p.xc1>=p.xc0 && p.yc1>=p.yc0 && p.zc1>=p.zc0 &&
        p.xc0>=0 && p.xc1<p.p4dImage->getXDim() &&
        p.yc0>=0 && p.yc1<p.p4dImage->getYDim() &&
        p.zc0>=0 && p.zc1<p.p4dImage->getZDim())
    {
        p4dImageNew = new Image4DSimple;
        if(!p4dImageNew->createImage(p.xc1-p.xc0+1, p.yc1-p.yc0+1, p.zc1-p.zc0+1, 1, p.p4dImage->getDatatype()))
            return false;
        
        if (p.b_brightfiled)
        {
            if(!invertedsubvolumecopy(p4dImageNew,
                                      p.p4dImage,
                                      p.xc0, p.xc1-p.xc0+1,
                                      p.yc0, p.yc1-p.yc0+1,
                                      p.zc0, p.zc1-p.zc0+1,
                                      p.channel, 1))
            return false;
        }
        else
        {
            if(!subvolumecopy(p4dImageNew,
                              p.p4dImage,
                              p.xc0, p.xc1-p.xc0+1,
                              p.yc0, p.yc1-p.yc0+1,
                              p.zc0, p.zc1-p.zc0+1,
                              p.channel, 1))
            return false;
        }
    }
    else
    {
        qDebug() << ("Somehow invalid volume box info is detected. Ignore it. But check your Vaa3D program.");
        return false;
    }
    
    //always just use the buffer data
    unsigned char * indata1d = p4dImageNew->getRawDataAtChannel(0);
    V3DLONG in_sz[4] = {p4dImageNew->getXDim(), p4dImageNew->getYDim(), p4dImageNew->getZDim(), 1};
    int datatype = p.p4dImage->getDatatype();

    int marker_thresh = INF;
    if(p.b_intensity)
    {
        if(p.b_brightfiled) p.bkg_thresh = 255 - p.bkg_thresh;

        for(int d = 1; d < p.landmarks.size(); d++)
        {
            int marker_x = p.landmarks[d].x - p.xc0;
            int marker_y = p.landmarks[d].y - p.yc0;
            int marker_z = p.landmarks[d].z - p.zc0;

            if(indata1d[marker_z*in_sz[0]*in_sz[1] + marker_y*in_sz[0] + marker_x] < marker_thresh)
            {
                marker_thresh = indata1d[marker_z*in_sz[0]*in_sz[1] + marker_y*in_sz[0] + marker_x];
            }
        }

        p.bkg_thresh = (marker_thresh - 10 > p.bkg_thresh) ? marker_thresh - 10 : p.bkg_thresh;
    }

    double dfactor_xy = 1, dfactor_z = 1;
    if(datatype != V3D_UINT8 || in_sz[0]>256 || in_sz[1]>256 || in_sz[2]>256)// && datatype != V3D_UINT16)
    {
        if (datatype!=V3D_UINT8)
        {
            if (!scale_img_and_convert28bit(p4dImageNew, 0, 255))
                return false;
            
            indata1d = p4dImageNew->getRawDataAtChannel(0);
            in_sz[0] = p4dImageNew->getXDim();
            in_sz[1] = p4dImageNew->getYDim();
            in_sz[2] = p4dImageNew->getZDim();
            in_sz[3] = p4dImageNew->getCDim();
            
            datatype = V3D_UINT8;
        }
        
        if (p.b_256cube)
        {
            if (in_sz[0]<=256 && in_sz[1]<=256 && in_sz[2]<=256)
            {
                dfactor_z = dfactor_xy = 1;
            }
            else if (in_sz[0] >= 2*in_sz[2] || in_sz[1] >= 2*in_sz[2])
            {
                if (in_sz[2]<=256)
                {
                    double MM = in_sz[0];
                    if (MM<in_sz[1]) MM=in_sz[1];
                    dfactor_xy = MM / 256.0;
                    dfactor_z = 1;
                }
                else
                {
                    double MM = in_sz[0];
                    if (MM<in_sz[1]) MM=in_sz[1];
                    if (MM<in_sz[2]) MM=in_sz[2];
                    dfactor_xy = dfactor_z = MM / 256.0;
                }
            }
            else
            {
                double MM = in_sz[0];
                if (MM<in_sz[1]) MM=in_sz[1];
                if (MM<in_sz[2]) MM=in_sz[2];
                dfactor_xy = dfactor_z = MM / 256.0;
            }
            
            if (dfactor_z>1 || dfactor_xy>1)
            {
                v3d_msg("enter ds code", 0);
                
                V3DLONG out_sz[4];
                unsigned char * outimg=0;
                if (!downsampling_img_xyz( indata1d, in_sz, dfactor_xy, dfactor_z, outimg, out_sz))
                    return false; //need to clean memory before return. a bug here
                
                p4dImageNew->setData(outimg, out_sz[0], out_sz[1], out_sz[2], out_sz[3], V3D_UINT8);
                
                indata1d = p4dImageNew->getRawDataAtChannel(0);
                in_sz[0] = p4dImageNew->getXDim();
                in_sz[1] = p4dImageNew->getYDim();
                in_sz[2] = p4dImageNew->getZDim();
                in_sz[3] = p4dImageNew->getCDim();
            }
        }
    }
    
    //QString outtmpfile = QString(p.p4dImage->getFileName()) + "_extract_tmp000.raw";
    //p4dImageNew->saveImage(qPrintable(outtmpfile));  v3d_msg(QString("save immediate input image to ") + outtmpfile, 0);

    
    if (p.bkg_thresh < 0)
    {
        if (p.channel >=0 && p.channel <= p.p4dImage->getCDim()-1)
        {
            double imgAve, imgStd;
            mean_and_std(p4dImageNew->getRawDataAtChannel(0), p4dImageNew->getTotalUnitNumberPerChannel(), imgAve, imgStd);
//            p.bkg_thresh = imgAve; //+0.5*imgStd ; //(imgAve < imgStd)? imgAve : (imgAve+imgStd)*.5;
            double td= (imgStd<10)? 10: imgStd;
            p.bkg_thresh = imgAve +0.5*td ; //(imgAve < imgStd)? imgAve : (imgAve+imgStd)*.5; //20170523, PHC
        }
        else
            p.bkg_thresh = 0;
    }
    else if (p.b_brightfiled)
    {
        p.bkg_thresh = 255 - p.bkg_thresh;
    }

    float * phi = 0;
    vector<MyMarker> inmarkers;
    for(i = 0; i < p.landmarks.size(); i++)
    {
        double x = p.landmarks[i].x - p.xc0 -1;
        double y = p.landmarks[i].y - p.yc0 -1;
        double z = p.landmarks[i].z - p.zc0 -1;
        
        //add scaling by PHC 121127
        x /= dfactor_xy;
        y /= dfactor_xy;
        z /= dfactor_z;
        
        inmarkers.push_back(MyMarker(x,y,z));
    }
    qint64 etime1 = timer1.elapsed();
    qDebug() << " **** neuron preprocessing takes [" << etime1 << " milliseconds]";
    
    v3d_msg("start neuron tracing for the preprocessed image.\n", 0);
    
    vector<MyMarker *> outtree;
    
    //add a timer by PHC 121005
    QElapsedTimer timer2;
    timer2.start();
    
    if(inmarkers.empty())
    {
        cout<<"Start detecting cellbody"<<endl;
		cout << "IMAGE DATATYPE: " << datatype << endl;
        switch(datatype)
        {
            case V3D_UINT8:
                fastmarching_dt_XY(indata1d, phi, in_sz[0], in_sz[1], in_sz[2],p.cnn_type, p.bkg_thresh);
                break;
            case V3D_UINT16:  //this is no longer needed, as the data type has been converted above
                fastmarching_dt_XY((short int*)indata1d, phi, in_sz[0], in_sz[1], in_sz[2],p.cnn_type, p.bkg_thresh);
                break;
        default:
            v3d_msg("Unsupported data type");
            break;
        }
        
        V3DLONG sz0 = in_sz[0];
        V3DLONG sz1 = in_sz[1];
        V3DLONG sz2 = in_sz[2];
        V3DLONG sz01 = sz0 * sz1;
        V3DLONG tol_sz = sz01 * sz2;
        
        V3DLONG max_loc = 0;
        double max_val = phi[0];
        for(V3DLONG i = 0; i < tol_sz; i++)
        {
            if(phi[i] > max_val)
            {
                max_val = phi[i];
                max_loc = i;
            }
        }
        MyMarker max_marker(max_loc % sz0, max_loc % sz01 / sz0, max_loc / sz01);
        inmarkers.push_back(max_marker);
    }
    
    cout<<"======================================="<<endl;
    cout<<"Construct the neuron tree"<<endl;
    if(inmarkers.empty())
    {
        cerr<<"need at least one markers"<<endl;
    }
    else if(inmarkers.size() == 1)
    {
        cout<<"only one input marker"<<endl;
        if(p.is_gsdt)
        {
            if(phi == 0)
            {
                cout<<"processing fastmarching distance transformation ..."<<endl;
                switch(datatype)
                {
                    case V3D_UINT8:
                        fastmarching_dt(indata1d, phi, in_sz[0], in_sz[1], in_sz[2], p.cnn_type, p.bkg_thresh);
                        break;
                    case V3D_UINT16:  //this is no longer needed, as the data type has been converted above
                        fastmarching_dt((short int *)indata1d, phi, in_sz[0], in_sz[1], in_sz[2], p.cnn_type, p.bkg_thresh);
                        break;
                    default:
                        v3d_msg("Unsupported data type");
                        break;
                }
            }
            
            cout<<endl<<"constructing fastmarching tree ..."<<endl;
            fastmarching_tree(inmarkers[0], phi, outtree, in_sz[0], in_sz[1], in_sz[2], p.cnn_type, p.bkg_thresh, p.is_break_accept);
        }
        else
        {
            switch(datatype)
            {
                case V3D_UINT8:
                    v3d_msg("8bit", 0);
                    fastmarching_tree(inmarkers[0], indata1d, outtree, in_sz[0], in_sz[1], in_sz[2], p.cnn_type, p.bkg_thresh, p.is_break_accept);
                    break;
                case V3D_UINT16: //this is no longer needed, as the data type has been converted above
                    v3d_msg("16bit", 0);
                    fastmarching_tree(inmarkers[0], (short int*)indata1d, outtree, in_sz[0], in_sz[1], in_sz[2], p.cnn_type, p.bkg_thresh, p.is_break_accept);
                    break;
                default:
                    v3d_msg("Unsupported data type");
                    break;
            }
        }
    }
    else
    {
        vector<MyMarker> target; target.insert(target.end(), inmarkers.begin()+1, inmarkers.end());
        if(p.is_gsdt)
        {
            if(phi == 0)
            {
                cout<<"processing fastmarching distance transformation ..."<<endl;
                switch(datatype)
                {
                    case V3D_UINT8:
                        fastmarching_dt(indata1d, phi, in_sz[0], in_sz[1], in_sz[2], p.cnn_type, p.bkg_thresh);
                        break;
                    case V3D_UINT16:
                        fastmarching_dt((short int *)indata1d, phi, in_sz[0], in_sz[1], in_sz[2], p.cnn_type, p.bkg_thresh);
                        break;
                }
            }
            cout<<endl<<"constructing fastmarching tree ..."<<endl;
            fastmarching_tree(inmarkers[0], target, phi, outtree, in_sz[0], in_sz[1], in_sz[2], p.cnn_type);
        }
        else
        {
            switch(datatype)
            {
                case V3D_UINT8:
                    fastmarching_tree(inmarkers[0], target, indata1d, outtree, in_sz[0], in_sz[1], in_sz[2], p.cnn_type);
                    break;
                case V3D_UINT16:
                    fastmarching_tree(inmarkers[0], target, (short int*) indata1d, outtree, in_sz[0], in_sz[1], in_sz[2], p.cnn_type);
                    break;
            }
        }
    }
    cout<<"======================================="<<endl;

    //save a copy of the ini tree
    cout<<"Save the initial unprunned tree"<<endl;
    vector<MyMarker*> & inswc = outtree;

    if (1)
    {
        V3DLONG tmpi;

        vector<MyMarker*> tmpswc;
        for (tmpi=0; tmpi<inswc.size(); tmpi++)
        {
            MyMarker * curp = new MyMarker(*(inswc[tmpi]));
            tmpswc.push_back(curp);

            if (dfactor_xy>1) inswc[tmpi]->x *= dfactor_xy;
            inswc[tmpi]->x += (p.xc0);
            if (dfactor_xy>1) inswc[tmpi]->x += dfactor_xy/2;

            if (dfactor_xy>1) inswc[tmpi]->y *= dfactor_xy;
            inswc[tmpi]->y += (p.yc0);
            if (dfactor_xy>1) inswc[tmpi]->y += dfactor_xy/2;

            if (dfactor_z>1) inswc[tmpi]->z *= dfactor_z;
            inswc[tmpi]->z += (p.zc0);
            if (dfactor_z>1)  inswc[tmpi]->z += dfactor_z/2;
        }

        //saveSWC_file(QString(p.p4dImage->getFileName()).append("_ini.swc").toStdString(), inswc, infostring);

        for (tmpi=0; tmpi<inswc.size(); tmpi++)
        {
            inswc[tmpi]->x = tmpswc[tmpi]->x;
            inswc[tmpi]->y = tmpswc[tmpi]->y;
            inswc[tmpi]->z = tmpswc[tmpi]->z;
        }

        for(tmpi = 0; tmpi < tmpswc.size(); tmpi++)
            delete tmpswc[tmpi];
        tmpswc.clear();
    }


    cout<<"Pruning neuron tree"<<endl;

    vector<MyMarker*> outswc;
    if(p.is_coverage_prune)
    {
        v3d_msg("start to use APP2 program.\n", 0);
        happ(inswc, outswc, indata1d, in_sz[0], in_sz[1], in_sz[2], p.bkg_thresh, p.length_thresh, p.SR_ratio);
    }
    else
    {
        hierarchy_prune(inswc, outswc, indata1d, in_sz[0], in_sz[1], in_sz[2], p.length_thresh);
        if(1) //get radius
        {
            double real_thres = 40; //PHC 20121011
            if (real_thres<p.bkg_thresh) real_thres = p.bkg_thresh;
            for(i = 0; i < outswc.size(); i++)
            {
                outswc[i]->radius = markerRadius(indata1d, in_sz, *(outswc[i]), real_thres);
            }
        }
    }
    
    qint64 etime2 = timer2.elapsed();
    qDebug() << " **** neuron tracing procedure takes [" << etime2 << " milliseconds]";
    
    if (p4dImageNew) {delete p4dImageNew; p4dImageNew=0;} //free buffer

    if(p.b_256cube)
    {
        inmarkers[0].x *= dfactor_xy;
        inmarkers[0].y *= dfactor_xy;
        inmarkers[0].z *= dfactor_z;

    }
    
    if(1)
    {
              
        for(i = 0; i < outswc.size(); i++) //add scaling 121127, PHC //add cutbox offset 121202, PHC
        {
            if (dfactor_xy>1) outswc[i]->x *= dfactor_xy;
            outswc[i]->x += (p.xc0);
            if (dfactor_xy>1) outswc[i]->x += dfactor_xy/2; //note that the offset corretion might not be accurate. PHC 121127

            if (dfactor_xy>1) outswc[i]->y *= dfactor_xy;
            outswc[i]->y += (p.yc0);
            if (dfactor_xy>1) outswc[i]->y += dfactor_xy/2;

            if (dfactor_z>1) outswc[i]->z *= dfactor_z;
            outswc[i]->z += (p.zc0);
            if (dfactor_z>1)  outswc[i]->z += dfactor_z/2;

            outswc[i]->radius *= dfactor_xy; //use xy for now
        }
        
        //re-estimate the radius using the original image
        double real_thres = 40; //PHC 20121011 //This should be rescaled later for datatypes that are not UINT8

        if (real_thres<p.bkg_thresh) real_thres = p.bkg_thresh;
        V3DLONG szOriginalData[4] = {p.p4dImage->getXDim(), p.p4dImage->getYDim(), p.p4dImage->getZDim(), 1};
        unsigned char * pOriginalData = (unsigned char *)(p.p4dImage->getRawDataAtChannel(p.channel));
        if(p.b_brightfiled)
        {
            for(V3DLONG i = 0; i < p.p4dImage->getTotalUnitNumberPerChannel(); i++)
                pOriginalData[i] = 255 - pOriginalData[i];

        }

        int method_radius_est = ( p.b_RadiusFrom2D ) ? 1 : 2;
        
        switch (p.p4dImage->getDatatype())
        {
            case V3D_UINT8:
            {
                for(i = 0; i < outswc.size(); i++)
                {
                    //printf(" node %ld of %ld.\n", i, outswc.size());
                    outswc[i]->radius = markerRadius(pOriginalData, szOriginalData, *(outswc[i]), real_thres, method_radius_est);
                }
            }
                break;
            case V3D_UINT16:
            {
                unsigned short int *pOriginalData_uint16 = (unsigned short int *)pOriginalData;
                for(i = 0; i < outswc.size(); i++)
                {
                    //printf(" node %ld of %ld.\n", i, outswc.size());
                    outswc[i]->radius = markerRadius(pOriginalData_uint16, szOriginalData, *(outswc[i]), real_thres * 16, method_radius_est); //*16 as it is often 12 bit data
                }
            }
                break;
            case V3D_FLOAT32:
            {
                float *pOriginalData_float = (float *)pOriginalData;
                for(i = 0; i < outswc.size(); i++)
                {
                    //printf(" node %ld of %ld.\n", i, outswc.size());
                    outswc[i]->radius = markerRadius(pOriginalData_float, szOriginalData, *(outswc[i]), real_thres, method_radius_est);
                }
            }
                break;
            default:
                break;
        }
        
        if(p.b_brightfiled)
        {
            for(V3DLONG i = 0; i < p.p4dImage->getTotalUnitNumberPerChannel(); i++)
                pOriginalData[i] = 255 - pOriginalData[i]; 
        }

        p.result = swc_convert(outswc);
    }

    //release memory
    if(phi){delete [] phi; phi = 0;}
    for(V3DLONG i = 0; i < outtree.size(); i++) delete outtree[i];
    outtree.clear();

    if (p.p4dImage) {delete p.p4dImage; p.p4dImage = nullptr;}
    
    return true;
}


bool PARA_APP2::fetch_para_commandline(const V3DPluginArgList &input, V3DPluginArgList &output, V3DPluginCallback2 &callback, QWidget *parent)
{
    vector<char*> * pinfiles = (input.size() >= 1) ? (vector<char*> *) input[0].p : 0;
    vector<char*> * poutfiles = (output.size() >= 1) ? (vector<char*> *) output[0].p : 0;
    vector<char*> * pparas = (input.size() >= 2) ? (vector<char*> *) input[1].p : 0;
    vector<char*> infiles = (pinfiles != 0) ? * pinfiles : vector<char*>();
    vector<char*> outfiles = (poutfiles != 0) ? * poutfiles : vector<char*>();
    vector<char*> paras = (pparas != 0) ? * pparas : vector<char*>();
    
    if(infiles.empty())
    {
        cerr<<"Need input image"<<endl;
        return false;
    }
    
    inimg_file = infiles[0];
    int k=0;
    inmarker_file = paras.empty() ? "" : paras[k]; if(inmarker_file == "NULL") inmarker_file = ""; k++;
  //  outswc_file = outfiles.empty() ? inimg_file + "_app2.swc" : outfiles[0];
    if(!outfiles.empty()) outswc_file = outfiles[0];
    //try to use as much as the default value in the PARA_APP2 constructor as possible
    channel = (paras.size() >= k+1) ? atoi(paras[k]) : channel;  k++;//0;
    bkg_thresh = (paras.size() >= k+1) ? atoi(paras[k]) : bkg_thresh; if(bkg_thresh == atoi("AUTO")) bkg_thresh = -1;k++;// 30;
    b_256cube = (paras.size() >= k+1) ? atoi(paras[k]) : b_256cube; k++;// true
    b_RadiusFrom2D = (paras.size() >= k+1) ? atoi(paras[k]) : b_RadiusFrom2D; k++;// true
    is_gsdt = (paras.size() >= k+1) ? atoi(paras[k]) : is_gsdt; k++;// true
    is_break_accept = (paras.size() >= k+1) ? atoi(paras[k]) : is_break_accept; k++;// true
    length_thresh = (paras.size() >= k+1) ? atof(paras[k]) : length_thresh; k++;// 1.0;
    b_resample = (paras.size() >= k+1) ? atoi(paras[k]) : b_resample; k++;// 1.0;
    b_brightfiled = (paras.size() >= k+1) ? atoi(paras[k]) : b_brightfiled; k++;// 0.0;
    b_intensity = (paras.size() >= k+1) ? atoi(paras[k]) : b_intensity; k++;// 0.0;

    b_menu = false;

    //cnn_type = 2; // default connection type 2
    //SR_ratio = 3.0/9.0;
    //is_coverage_prune = true;
    //is_break_accept = false;
    
    return true;
}

