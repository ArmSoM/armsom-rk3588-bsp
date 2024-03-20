#include "RawStreamCapUnit.h"
#include "rkcif-config.h"
#include "MediaInfo.h"
#include "xcam_defs.h"

namespace RkRawStream {
RawStreamCapUnit::RawStreamCapUnit (char *dev0, char *dev1, char *dev2)
    :_skip_num(0)
    ,_mipi_dev_max(0)
    ,_state(RAW_CAP_STATE_INVALID)
    ,_memory_type(V4L2_MEMORY_MMAP)
    ,user_on_frame_capture_cb(NULL)
{
    if(dev0){
        LOGD_RKSTREAM( "%s open device %s", __FUNCTION__, dev0);
        _dev[0] = new V4l2Device (dev0);
        _dev[0]->open();
        _dev[0]->set_mem_type(_memory_type);
        _mipi_dev_max++;
    }
    if(dev1){
        LOGD_RKSTREAM( "%s open device %s", __FUNCTION__, dev1);
        _dev[1] = new V4l2Device (dev1);
        _dev[1]->open();
        _dev[1]->set_mem_type(_memory_type);
        _mipi_dev_max++;
    }
    if(dev2){
        LOGD_RKSTREAM( "%s open device %s", __FUNCTION__, dev2);
        _dev[2] = new V4l2Device (dev2);
        _dev[2]->open();
        _dev[2]->set_mem_type(_memory_type);
        _mipi_dev_max++;
    }

    for (int i = 0; i < _mipi_dev_max; i++) {
        if (_dev[i].ptr())
            _dev[i]->set_buffer_count(STREAM_VIPCAP_BUF_NUM);

        if (_dev[i].ptr())
            _dev[i]->set_buf_sync (true);

        _dev_bakup[i] = _dev[i];
        _dev_index[i] = i;
        _stream[i] =  new RKRawStream(_dev[i], i, ISP_POLL_TX);
        _stream[i]->setPollCallback(this);
    }
    _sensor_dev = NULL;
    _state = RAW_CAP_STATE_INITED;
}

RawStreamCapUnit::RawStreamCapUnit (const rk_sensor_full_info_t *s_info)
    :_skip_num(0)
    ,_mipi_dev_max(0)
    ,_state(RAW_CAP_STATE_INVALID)
    ,_memory_type(V4L2_MEMORY_MMAP)
    ,user_on_frame_capture_cb(NULL)
{
    bool linked_to_isp = s_info->linked_to_isp;

    strncpy(_sns_name, s_info->sensor_name.c_str(), 32);

    /*
     * for _mipi_tx_devs, index 0 refer to short frame always, inedex 1 refer
     * to middle frame always, index 2 refert to long frame always.
     * for CIF usecase, because mipi_id0 refert to long frame always, so we
     * should know the HDR mode firstly befor building the relationship between
     * _mipi_tx_devs array and mipi_idx. here we just set the mipi_idx to
     * _mipi_tx_devs, we will build the real relation in start.
     * for CIF usecase, rawwr2_path is always connected to _mipi_tx_devs[0],
     * rawwr0_path is always connected to _mipi_tx_devs[1], and rawwr1_path is always
     * connected to _mipi_tx_devs[0]
     */
    //short frame
    if (strlen(s_info->isp_info->rawrd2_s_path)) {
        if (linked_to_isp)
            _dev[0] = new V4l2Device (s_info->isp_info->rawwr2_path);//rkisp_rawwr2
        else {
            if (s_info->dvp_itf) {
                if (strlen(s_info->cif_info->stream_cif_path))
                    _dev[0] = new V4l2Device (s_info->cif_info->stream_cif_path);
                else
                    _dev[0] = new V4l2Device (s_info->cif_info->dvp_id0);
            } else{
                _dev[0] = new V4l2Device (s_info->cif_info->mipi_id0);
            }
        }
        _dev[0]->open();
        _dev[0]->set_mem_type(_memory_type);
    }
    //mid frame
    if (strlen(s_info->isp_info->rawrd0_m_path)) {
        if (linked_to_isp)
            _dev[1] = new V4l2Device (s_info->isp_info->rawwr0_path);//rkisp_rawwr0
        else {
            if (!s_info->dvp_itf)
                _dev[1] = new V4l2Device (s_info->cif_info->mipi_id1);
        }

        if (_dev[1].ptr()){
            _dev[1]->open();
            _dev[1]->set_mem_type(_memory_type);
        }
    }
    //long frame
    if (strlen(s_info->isp_info->rawrd1_l_path)) {
        if (linked_to_isp)
            _dev[2] = new V4l2Device (s_info->isp_info->rawwr1_path);//rkisp_rawwr1
        else {
            if (!s_info->dvp_itf)
                _dev[2] = new V4l2Device (s_info->cif_info->mipi_id2);//rkisp_rawwr1
        }
        if (_dev[2].ptr()){
            _dev[2]->open();
            _dev[2]->set_mem_type(_memory_type);
        }
    }
    for (int i = 0; i < 3; i++) {
        if (linked_to_isp) {
            if (_dev[i].ptr())
                _dev[i]->set_buffer_count(STREAM_ISP_BUF_NUM);
        } else {
            if (_dev[i].ptr())
                _dev[i]->set_buffer_count(STREAM_VIPCAP_BUF_NUM);
        }
        if (_dev[i].ptr())
            _dev[i]->set_buf_sync (true);

        _dev_bakup[i] = _dev[i];
        _dev_index[i] = i;
        _stream[i] =  new RKRawStream(_dev[i], i, ISP_POLL_TX);
        _stream[i]->setPollCallback(this);
    }


    _sensor_dev = new V4l2SubDevice(s_info->device_name.c_str());
    _sensor_dev->open();
    _state = RAW_CAP_STATE_INITED;

    is_multi_isp_mode = s_info->isp_info->is_multi_isp_mode;
}

void
RawStreamCapUnit::set_devices(RawStreamProcUnit *proc)
{
    _proc_stream = proc;
}

RawStreamCapUnit::~RawStreamCapUnit ()
{
    _state = RAW_CAP_STATE_INVALID;
}

XCamReturn RawStreamCapUnit::start()
{
    LOGD_RKSTREAM( "%s enter", __FUNCTION__);
    for (int i = 0; i < _mipi_dev_max; i++) {
        _stream[i]->start();
    }
    _state = RAW_CAP_STATE_STARTED;
    LOGD_RKSTREAM( "%s exit", __FUNCTION__);
    return XCAM_RETURN_NO_ERROR;
}

XCamReturn RawStreamCapUnit::stop ()
{
    LOGD_RKSTREAM( "%s enter", __FUNCTION__);
    for (int i = 0; i < _mipi_dev_max; i++) {
        _stream[i]->stopThreadOnly();
    }
    _buf_mutex.lock();
    for (int i = 0; i < _mipi_dev_max; i++) {
        buf_list[i].clear ();
    }
    for (int i = 0; i < _mipi_dev_max; i++) {
        user_used_buf_list[i].clear ();
    }
    _buf_mutex.unlock();
    for (int i = 0; i < _mipi_dev_max; i++) {
        _stream[i]->stopDeviceOnly();
    }
    _state = RAW_CAP_STATE_STOPPED;
    LOGD_RKSTREAM( "%s exit", __FUNCTION__);
    return XCAM_RETURN_NO_ERROR;
}

XCamReturn RawStreamCapUnit::stop_device ()
{
    LOGD_RKSTREAM( "%s enter", __FUNCTION__);
    for (int i = 0; i < _mipi_dev_max; i++) {
        _stream[i]->stopThreadOnly();
    }
    for (int i = 0; i < _mipi_dev_max; i++) {
        _stream[i]->stopDeviceStreamoff();
    }
    _state = RAW_CAP_STATE_STOPPED;
    LOGD_RKSTREAM( "%s exit", __FUNCTION__);
    return XCAM_RETURN_NO_ERROR;
}

XCamReturn RawStreamCapUnit::release_buffer ()
{
    LOGD_RKSTREAM( "%s enter", __FUNCTION__);
    _buf_mutex.lock();
    for (int i = 0; i < _mipi_dev_max; i++) {
        buf_list[i].clear ();
    }
    for (int i = 0; i < _mipi_dev_max; i++) {
        user_used_buf_list[i].clear ();
    }
    _buf_mutex.unlock();
    for (int i = 0; i < _mipi_dev_max; i++) {
        _stream[i]->stopDeviceFreebuffer();
    }
    _state = RAW_CAP_STATE_STOPPED;
    LOGD_RKSTREAM( "%s exit", __FUNCTION__);
    return XCAM_RETURN_NO_ERROR;
}


XCamReturn
RawStreamCapUnit::prepare(int idx, uint8_t buf_memory_type, uint8_t buf_cnt)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    _memory_type = (enum v4l2_memory)buf_memory_type;
    LOGE_RKSTREAM("RawStreamCapUnit::prepare idx:%d buf_memory_type: %d\n",idx, buf_memory_type);
    // mipi rx/tx format should match to sensor.
    for (int i = 0; i < 3; i++) {
        if (!(idx & (1 << i)))
            continue;

        if(buf_memory_type)
            _dev[i]->set_mem_type(_memory_type);

        if(buf_cnt)
            _dev[i]->set_buffer_count(buf_cnt);

        ret = _dev[i]->prepare();
        if (ret < 0)
            LOGE_RKSTREAM( "mipi tx:%d prepare err: %d\n", i, ret);

        _stream[i]->set_device_prepared(true);
    }
    _state = RAW_CAP_STATE_PREPARED;
    LOGD_RKSTREAM( "%s exit", __FUNCTION__);
    return ret;
}

void
RawStreamCapUnit::set_working_mode(int mode)
{
    LOGD_RKSTREAM( "%s enter,mode=0x%x", __FUNCTION__, mode);
    _working_mode = mode;

    switch (_working_mode) {
    case RK_AIQ_ISP_HDR_MODE_3_FRAME_HDR:
    case RK_AIQ_ISP_HDR_MODE_3_LINE_HDR:
        _mipi_dev_max = 3;
        break;
    case RK_AIQ_ISP_HDR_MODE_2_FRAME_HDR:
    case RK_AIQ_ISP_HDR_MODE_2_LINE_HDR:
        _mipi_dev_max = 2;
        break;
    default:
        _mipi_dev_max = 1;
    }
    LOGD_RKSTREAM( "%s exit", __FUNCTION__);
}

void
RawStreamCapUnit::set_tx_format(uint32_t width, uint32_t height, uint32_t pix_fmt, int mode)
{
    struct v4l2_format format;
    memset(&format, 0, sizeof(format));

    for (int i = 0; i < 3; i++) {
        if (_dev[i].ptr()){
            _dev[i]->get_format (format);

            int bpp = pixFmt2Bpp(format.fmt.pix.pixelformat);
            int mem_mode = mode;
            int ret1 = _dev[i]->io_control (RKCIF_CMD_SET_CSI_MEMORY_MODE, &mem_mode);
            if (ret1)
                LOGE_RKSTREAM("set RKCIF_CMD_SET_CSI_MEMORY_MODE failed !\n");

            LOGI_RKSTREAM("set_tx_format: setup fmt %dx%d, 0x%x mem_mode %d\n",width, height, format.fmt.pix.pixelformat, mem_mode);
            _dev[i]->set_format(width,
                                height,
                                format.fmt.pix.pixelformat,
                                V4L2_FIELD_NONE,
                                0);
        }
    }
}

void
RawStreamCapUnit::set_sensor_format(uint32_t width, uint32_t height, uint32_t fps)
{
    if(_sensor_dev.ptr()){
        struct v4l2_subdev_format format;
        memset(&format, 0, sizeof(format));
        _sensor_dev->getFormat(format);


        format.pad = 0;
        format.which = V4L2_SUBDEV_FORMAT_ACTIVE;
        format.format.width = width;
        format.format.height = height;
        _sensor_dev->setFormat(format);
    }
}

void
RawStreamCapUnit::set_sensor_mode(uint32_t mode)
{
    rkmodule_hdr_cfg hdr_cfg;
    __u32 hdr_mode = NO_HDR;
    if(_sensor_dev.ptr()){
        switch (mode) {
        case RK_AIQ_ISP_HDR_MODE_3_FRAME_HDR:
        case RK_AIQ_ISP_HDR_MODE_3_LINE_HDR:
            hdr_mode = HDR_X3;
            break;
        case RK_AIQ_ISP_HDR_MODE_2_FRAME_HDR:
        case RK_AIQ_ISP_HDR_MODE_2_LINE_HDR:
            hdr_mode = HDR_X2;
            break;
        default:
            hdr_mode = NO_HDR;
        }

        hdr_cfg.hdr_mode = hdr_mode;
        if (_sensor_dev->io_control(RKMODULE_SET_HDR_CFG, &hdr_cfg) < 0) {
            LOGE_RKSTREAM("set_sensor_mode failed to set hdr mode %d\n", hdr_mode);
            //return XCAM_RETURN_ERROR_IOCTL;
        }
    }
}

/*
void
RawStreamCapUnit::prepare_cif_mipi()
{
    LOGD_RKSTREAM( "%s enter,working_mode=0x%x", __FUNCTION__, _working_mode);

    FakeV4l2Device* fake_dev = dynamic_cast<FakeV4l2Device* >(_dev[0].ptr());

    if (fake_dev) {
        LOGD_RKSTREAM("ignore fake tx");
        return;
    }

    SmartPtr<V4l2Device> tx_devs_tmp[3] =
    {
        _dev_bakup[0],
        _dev_bakup[1],
        _dev_bakup[2],
    };

    // _mipi_tx_devs
    if (_working_mode == RK_AIQ_WORKING_MODE_NORMAL) {
        // use _mipi_tx_devs[0] only
        // id0 as normal
        // do nothing
        LOGD_RKSTREAM( "CIF tx: %s -> normal",
                        _dev[0]->get_device_name());
    } else if (RK_AIQ_HDR_GET_WORKING_MODE(_working_mode) == RK_AIQ_WORKING_MODE_ISP_HDR2) {
        // use _mipi_tx_devs[0] and _mipi_tx_devs[1]
        // id0 as l, id1 as s
        SmartPtr<V4l2Device> tmp = tx_devs_tmp[1];
        tx_devs_tmp[1] = tx_devs_tmp[0];
        tx_devs_tmp[0] = tmp;
        LOGD_RKSTREAM( "CIF tx: %s -> long",
                        _dev[1]->get_device_name());
        LOGD_RKSTREAM( "CIF tx: %s -> short",
                        _dev[0]->get_device_name());
    } else if (RK_AIQ_HDR_GET_WORKING_MODE(_working_mode) == RK_AIQ_WORKING_MODE_ISP_HDR3) {
        // use _mipi_tx_devs[0] and _mipi_tx_devs[1]
        // id0 as l, id1 as m, id2 as s
        SmartPtr<V4l2Device> tmp = tx_devs_tmp[2];
        tx_devs_tmp[2] = tx_devs_tmp[0];
        tx_devs_tmp[0] = tmp;
        LOGD_RKSTREAM( "CIF tx: %s -> long",
                        _dev[2]->get_device_name());
        LOGD_RKSTREAM( "CIF tx: %s -> middle",
                        _dev[1]->get_device_name());
        LOGD_RKSTREAM( "CIF tx: %s -> short",
                        _dev[0]->get_device_name());
    } else {
        LOGE( "wrong hdr mode: %d\n", _working_mode);
    }
    for (int i = 0; i < 3; i++) {
        _dev[i] = tx_devs_tmp[i];
        _dev_index[i] = i;
        _stream[i].release();
        _stream[i] =  new RKRawStream(_dev[i], i, ISP_POLL_TX);
        _stream[i]->setPollCallback(this);
    }
    LOGD_RKSTREAM( "%s exit", __FUNCTION__);
}
*/

XCamReturn
RawStreamCapUnit::poll_buffer_ready (SmartPtr<V4l2BufferProxy> &buf, int dev_index)
{
    XCamReturn ret = XCAM_RETURN_NO_ERROR;
    SmartPtr<V4l2BufferProxy> buf_s, buf_m, buf_l;
    LOGD_RKSTREAM( "%s enter,dev_index=0x%x", __FUNCTION__, dev_index);

    _buf_mutex.lock();
    buf_list[dev_index].push(buf);
    ret = sync_raw_buf(buf_s, buf_m, buf_l);
    _buf_mutex.unlock();


    if (ret == XCAM_RETURN_NO_ERROR) {
        //if (_proc_stream)
        //    _proc_stream->send_sync_buf(buf_s, buf_m, buf_l);

        if (user_on_frame_capture_cb){
            struct timespec tx_timestamp;
            int tx_timems;
            user_takes_buf = false;

            do_capture_callback(buf_s, buf_m, buf_l);

            clock_gettime(CLOCK_MONOTONIC, &tx_timestamp);
            tx_timems = XCAM_TIMESPEC_2_USEC(tx_timestamp) / 1000;
            LOGI_RKSTREAM("BUFDEBUG vicapdq [%s] index %d  seq %d tx_time %d", _sns_name, buf_s->get_v4l2_buf_index(), buf_s->get_sequence(),tx_timems);
            if(user_takes_buf){
                switch (_working_mode) {
                case RK_AIQ_ISP_HDR_MODE_3_FRAME_HDR:
                case RK_AIQ_ISP_HDR_MODE_3_LINE_HDR:
                    user_used_buf_list[0].push(buf_s);
                    user_used_buf_list[1].push(buf_m);
                    user_used_buf_list[2].push(buf_l);
                    break;
                case RK_AIQ_ISP_HDR_MODE_2_FRAME_HDR:
                case RK_AIQ_ISP_HDR_MODE_2_LINE_HDR:
                    user_used_buf_list[0].push(buf_s);
                    user_used_buf_list[1].push(buf_m);
                    break;
                default:
                    user_used_buf_list[0].push(buf_s);
                }
            }
        }
/*
        if (_camHw->mHwResLintener) {
            struct VideoBufferInfo vbufInfo;
            vbufInfo.init(_format.fmt.pix.pixelformat, _format.fmt.pix.width, _format.fmt.pix.height,
                         _format.fmt.pix.width, _format.fmt.pix.height, _format.fmt.pix.sizeimage, true);
            SmartPtr<SubVideoBuffer> subvbuf = new SubVideoBuffer (buf_s);
            subvbuf->_buf_type = ISP_POLL_TX;
            subvbuf->set_sequence(buf_s->get_sequence());
            subvbuf->set_video_info(vbufInfo);
            SmartPtr<VideoBuffer> vbuf = subvbuf.dynamic_cast_ptr<VideoBuffer>();
            _camHw->mHwResLintener->hwResCb(vbuf);
        }
*/
    }

    return XCAM_RETURN_NO_ERROR;
}

XCamReturn
RawStreamCapUnit::sync_raw_buf
(
    SmartPtr<V4l2BufferProxy> &buf_s,
    SmartPtr<V4l2BufferProxy> &buf_m,
    SmartPtr<V4l2BufferProxy> &buf_l
)
{
    uint32_t sequence_s = -1, sequence_m = -1, sequence_l = -1;

    for (int i = 0; i < _mipi_dev_max; i++) {
        if (buf_list[i].is_empty()) {
            return XCAM_RETURN_ERROR_FAILED;
        }
    }

    buf_l = buf_list[ISP_MIPI_HDR_L].front();
    if (buf_l.ptr())
        sequence_l = buf_l->get_sequence();

    buf_m = buf_list[ISP_MIPI_HDR_M].front();
    if (buf_m.ptr())
        sequence_m = buf_m->get_sequence();

    buf_s = buf_list[ISP_MIPI_HDR_S].front();

    if (buf_s.ptr()) {
        sequence_s = buf_s->get_sequence();
        if ((_working_mode == RK_AIQ_ISP_HDR_MODE_3_FRAME_HDR ||
                _working_mode == RK_AIQ_ISP_HDR_MODE_3_LINE_HDR) &&
                buf_m.ptr() && buf_l.ptr() && buf_s.ptr() &&
                sequence_l == sequence_s && sequence_m == sequence_s) {

            buf_list[ISP_MIPI_HDR_S].erase(buf_s);
            buf_list[ISP_MIPI_HDR_M].erase(buf_m);
            buf_list[ISP_MIPI_HDR_L].erase(buf_l);
            //if (check_skip_frame(sequence_s)) {
            //    LOGW_RKSTREAM( "skip frame %d", sequence_s);
            //    goto end;
            //}
        } else if ((_working_mode == RK_AIQ_ISP_HDR_MODE_2_FRAME_HDR ||
                    _working_mode == RK_AIQ_ISP_HDR_MODE_2_LINE_HDR) &&
                   buf_m.ptr() && buf_s.ptr() && sequence_m == sequence_s) {
            buf_list[ISP_MIPI_HDR_S].erase(buf_s);
            buf_list[ISP_MIPI_HDR_M].erase(buf_m);
            //if (check_skip_frame(sequence_s)) {
            //    LOGE_RKSTREAM( "skip frame %d", sequence_s);
            //    goto end;
            //}
        } else if (_working_mode == RK_AIQ_WORKING_MODE_NORMAL) {
            buf_list[ISP_MIPI_HDR_S].erase(buf_s);
            //if (check_skip_frame(sequence_s)) {
            //    LOGW_RKSTREAM( "skip frame %d", sequence_s);
            //    goto end;
            //}
        } else {
            LOGW_RKSTREAM( "do nothing, sequence not match l: %d, s: %d, m: %d !!!",
                            sequence_l, sequence_s, sequence_m);
        }
        return XCAM_RETURN_NO_ERROR;
    }
end:
    return XCAM_RETURN_ERROR_FAILED;
}

void rkraw_append_buf(uint8_t *p, uint16_t tag, SmartPtr<V4l2BufferProxy> &buf)
{
    struct _live_rkraw_buf *b = (struct _live_rkraw_buf *)p;
    uint64_t uptr = buf->get_v4l2_userptr();

    b->_header.block_id = tag;
    b->_header.block_length = sizeof(struct _st_addrinfo_stream);
    b->_addr.idx = buf->get_v4l2_buf_index();
    b->_addr.fd = buf->get_expbuf_fd();
    b->_addr.size = buf->get_v4l2_buf_planar_length(0);
    b->_addr.timestamp = buf->get_timestamp();
    b->_addr.haddr = uptr >> 32;
    b->_addr.laddr = uptr & 0xFFFFFFFF;
}

XCamReturn
RawStreamCapUnit::do_capture_callback
(
    SmartPtr<V4l2BufferProxy> &buf_s,
    SmartPtr<V4l2BufferProxy> &buf_m,
    SmartPtr<V4l2BufferProxy> &buf_l
)
{
    struct v4l2_buffer *vbuf[3];
    struct v4l2_format *vfmt[3];
    // int vfd[3];
    // int state = -1;

    int index = buf_s->get_v4l2_buf_index();
    if(index > STREAM_VIPCAP_BUF_NUM-1){
        LOGW_RKSTREAM("do_capture_callback: bad index %d!",index);
    }

    uint8_t *p = (uint8_t *)&_rkraw_data[index];
    uint16_t *tag = (uint16_t *)p;
    p = p + 2;
    *tag = START_TAG;

    struct _raw_format *f = (struct _raw_format *)p;
    p = p + sizeof(struct _raw_format);
    f->tag = FORMAT_TAG;
    f->size = sizeof(struct _raw_format) - sizeof(struct _block_header);
    f->vesrion = 0x0200;
    memcpy(f->sensor, _sns_name, 32);
    memset(f->scene, 0, 32);
    f->frame_id = buf_s->get_sequence();
    f->width = _format.fmt.pix.width;
    f->height = _format.fmt.pix.height;

    //TODO: use correct fmt
    f->bit_width = 0;
    f->bayer_fmt = 0;

    if ((_working_mode == RK_AIQ_ISP_HDR_MODE_3_FRAME_HDR ||
            _working_mode == RK_AIQ_ISP_HDR_MODE_3_LINE_HDR)) {
        f->hdr_mode = 3;

        rkraw_append_buf(p, HDR_S_RAW_TAG, buf_s);
        p = p + sizeof(struct _live_rkraw_buf);

        rkraw_append_buf(p, HDR_M_RAW_TAG, buf_m);
        p = p + sizeof(struct _live_rkraw_buf);

        rkraw_append_buf(p, HDR_L_RAW_TAG, buf_l);
        p = p + sizeof(struct _live_rkraw_buf);


        // vbuf[0] = (v4l2_buffer*) &buf_s->get_v4l2_buf();
        // vbuf[1] = (v4l2_buffer*) &buf_m->get_v4l2_buf();
        // vbuf[2] = (v4l2_buffer*) &buf_l->get_v4l2_buf();
        // vfmt[0] = (v4l2_format*) &buf_s->get_v4l2_format();
        // vfmt[1] = (v4l2_format*) &buf_m->get_v4l2_format();
        // vfmt[2] = (v4l2_format*) &buf_l->get_v4l2_format();
        // vfd[0] = buf_s->get_expbuf_fd();
        // vfd[1] = buf_m->get_expbuf_fd();
        // vfd[2] = buf_l->get_expbuf_fd();

        // state = 3;

    } else if ((_working_mode == RK_AIQ_ISP_HDR_MODE_2_FRAME_HDR ||
                _working_mode == RK_AIQ_ISP_HDR_MODE_2_LINE_HDR)) {
        f->hdr_mode = 2;

        rkraw_append_buf(p, HDR_S_RAW_TAG, buf_s);
        p = p + sizeof(struct _live_rkraw_buf);

        rkraw_append_buf(p, HDR_M_RAW_TAG, buf_m);
        p = p + sizeof(struct _live_rkraw_buf);

        // vbuf[0] = (v4l2_buffer*) &buf_s->get_v4l2_buf();
        // vbuf[1] = (v4l2_buffer*) &buf_m->get_v4l2_buf();
        // vbuf[2] = NULL;
        // vfmt[0] = (v4l2_format*) &buf_s->get_v4l2_format();
        // vfmt[1] = (v4l2_format*) &buf_m->get_v4l2_format();
        // vfmt[2] = NULL;
        // vfd[0] = buf_s->get_expbuf_fd();
        // vfd[1] = buf_m->get_expbuf_fd();

        // state = 2;

    } else if (_working_mode == RK_AIQ_WORKING_MODE_NORMAL) {
        f->hdr_mode = 1;

        rkraw_append_buf(p, NORMAL_RAW_TAG, buf_s);
        p = p + sizeof(struct _live_rkraw_buf);


        // vbuf[0] = (v4l2_buffer*) &buf_s->get_v4l2_buf();
        // vbuf[1] = NULL;
        // vbuf[2] = NULL;
        // vfmt[0] = (v4l2_format*) &buf_s->get_v4l2_format();
        // vfmt[1] = NULL;
        // vfmt[2] = NULL;
        // vfd[0] = buf_s->get_expbuf_fd();

        // state = 1;

    }

    tag = (uint16_t *)p;
    p = p + 2;
    *tag = END_TAG;

    uint32_t rkraw_len = p - (uint8_t *)&_rkraw_data[index];
    if(user_on_frame_capture_cb){
        user_on_frame_capture_cb((uint8_t *)&_rkraw_data[index], rkraw_len);
        //user_on_frame_capture_cb(vbuf, vfmt, vfd, state);
    }

    return XCAM_RETURN_NO_ERROR;
}

void RawStreamCapUnit::release_user_taked_buf(int dev_index)
{
    _buf_mutex.lock();
    if (!user_used_buf_list[dev_index].is_empty()) {
        SmartPtr<V4l2BufferProxy> rx_buf = user_used_buf_list[dev_index].pop(-1);
        struct timespec rx_timestamp;
        clock_gettime(CLOCK_MONOTONIC, &rx_timestamp);
        int64_t rx_timems = XCAM_TIMESPEC_2_USEC(rx_timestamp)  / 1000;
        LOGI_RKSTREAM("BUFDEBUG vicapq [%s] index %d  seq %d  rx_timems %ld \n", _sns_name,
            rx_buf->get_v4l2_buf_index(), rx_buf->get_sequence(),rx_timems);
    }
    _buf_mutex.unlock();
}

void RawStreamCapUnit::set_dma_buf(int dev_index, int buf_index, int fd)
{
    int ret, i;
    struct v4l2_format format;
    i = dev_index;
    SmartPtr<V4l2Buffer> v4l2buf;

    ret = _dev[i]->get_buffer(v4l2buf, buf_index);
    if (ret != XCAM_RETURN_NO_ERROR) {
        LOGE_RKSTREAM( "set_dma_buf can not get buffer\n", i);
        return;
    }
    v4l2buf->set_expbuf_fd((const int)fd);
    ret = _dev[i]->queue_buffer(v4l2buf);
    if (ret != XCAM_RETURN_NO_ERROR) {
        LOGE_RKSTREAM( "set_dma_buf queue buffer failed\n", i);
    }
    return;
}

}
