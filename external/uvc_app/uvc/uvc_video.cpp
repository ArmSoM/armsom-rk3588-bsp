/*
 * Copyright (C) 2019 Rockchip Electronics Co., Ltd.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL), available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "uvc_video.h"
#include "uvc-gadget.h"
#include "yuv.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>

#include <list>

struct uvc_buffer {
    void* buffer;
    size_t size;
    size_t total_size;
    int width;
    int height;
    int video_id;
};

struct uvc_buffer_list {
    std::list<struct uvc_buffer*> buffer_list;
    pthread_mutex_t mutex;
};

struct video_uvc {
    struct uvc_buffer_list write;
    struct uvc_buffer_list read;
    pthread_t id;
    bool run;
    int video_id;
};

static std::list<struct uvc_video*> lst_v;
static pthread_mutex_t mtx_v = PTHREAD_MUTEX_INITIALIZER;


static struct uvc_buffer* uvc_buffer_create(int width, int height, int id)
{
    struct uvc_buffer* buffer = NULL;

    buffer = (struct uvc_buffer*)calloc(1, sizeof(struct uvc_buffer));
    if (!buffer)
        return NULL;
    buffer->width = width;
    buffer->height = height;
    buffer->size = buffer->width * buffer->height * 2;
    buffer->buffer = calloc(1, buffer->size);
    if (!buffer->buffer) {
        free(buffer);
        return NULL;
    }
    buffer->total_size = buffer->size;
    buffer->video_id = id;
    return buffer;
}

static void uvc_buffer_push_back(struct uvc_buffer_list* uvc_buffer,
                                 struct uvc_buffer* buffer)
{
    pthread_mutex_lock(&uvc_buffer->mutex);
    uvc_buffer->buffer_list.push_back(buffer);
    pthread_mutex_unlock(&uvc_buffer->mutex);
}

static struct uvc_buffer* uvc_buffer_pop_front(
    struct uvc_buffer_list* uvc_buffer)
{
    struct uvc_buffer* buffer = NULL;

    pthread_mutex_lock(&uvc_buffer->mutex);
    if (!uvc_buffer->buffer_list.empty()) {
        buffer = uvc_buffer->buffer_list.front();
        uvc_buffer->buffer_list.pop_front();
    }
    pthread_mutex_unlock(&uvc_buffer->mutex);
    return buffer;
}

static struct uvc_buffer* uvc_buffer_front(struct uvc_buffer_list* uvc_buffer)
{
    struct uvc_buffer* buffer = NULL;

    pthread_mutex_lock(&uvc_buffer->mutex);
    if (!uvc_buffer->buffer_list.empty())
        buffer = uvc_buffer->buffer_list.front();
    else
        buffer = NULL;
    pthread_mutex_unlock(&uvc_buffer->mutex);
    return buffer;
}

static void uvc_buffer_destroy(struct uvc_buffer_list* uvc_buffer)
{
    struct uvc_buffer* buffer = NULL;

    pthread_mutex_lock(&uvc_buffer->mutex);
    while (!uvc_buffer->buffer_list.empty()) {
        buffer = uvc_buffer->buffer_list.front();
        free(buffer->buffer);
        free(buffer);
        uvc_buffer->buffer_list.pop_front();
    }
    pthread_mutex_unlock(&uvc_buffer->mutex);
    pthread_mutex_destroy(&uvc_buffer->mutex);
}

static void uvc_buffer_clear(struct uvc_buffer_list* uvc_buffer)
{
    pthread_mutex_lock(&uvc_buffer->mutex);
    uvc_buffer->buffer_list.clear();
    pthread_mutex_unlock(&uvc_buffer->mutex);
}

static void* uvc_gadget_pthread(void* arg)
{
    int *id = (int *)arg;

    prctl(PR_SET_NAME, "uvc_gadget_pthread", 0, 0, 0);

    uvc_gadget_main(*id);
    uvc_set_user_run_state(true, *id);
    pthread_exit(NULL);
}

int uvc_gadget_pthread_create(int *id)
{
    pthread_t *pid = NULL;

    uvc_memset_uvc_user(*id);
    if ((pid = uvc_video_get_uvc_pid(*id))) {
        if (pthread_create(pid, NULL, uvc_gadget_pthread, id)) {
            printf("create uvc_gadget_pthread fail!\n");
            return -1;
        }
    }
    return 0;
}

static int _uvc_video_id_check(int id)
{
    int ret = 0;

    if (!lst_v.empty()) {
        for (std::list<struct uvc_video*>::iterator i = lst_v.begin(); i != lst_v.end(); ++i) {
            struct uvc_video* l = *i;
            if (id == l->id) {
                ret = -1;
                break;
            }
        }
    }

    return ret;
}

int uvc_video_id_check(int id)
{
    int ret = 0;

    pthread_mutex_lock(&mtx_v);
    ret = _uvc_video_id_check(id);
    pthread_mutex_unlock(&mtx_v);

    return ret;
}

int uvc_video_id_add(int id)
{
    int ret = 0;

    printf("add uvc video id: %d\n", id);

    pthread_mutex_lock(&mtx_v);
    if (!_uvc_video_id_check(id)) {
        struct uvc_video* v = (struct uvc_video*)calloc(1, sizeof(struct uvc_video));
        if (v) {
            v->id = id;
            lst_v.push_back(v);
            pthread_mutex_unlock(&mtx_v);
            uvc_gadget_pthread_create(&v->id);
            pthread_mutex_lock(&mtx_v);
            pthread_mutex_init(&v->buffer_mutex, NULL);
            pthread_mutex_init(&v->user_mutex, NULL);
            ret = 0;
        } else {
            printf("%s: %d: memory alloc fail.\n", __func__, __LINE__);
            ret = -1;
        }
    } else {
        printf("%s: %d: %d already exist.\n", __func__, __LINE__, id);
        ret = -1;
    }
    pthread_mutex_unlock(&mtx_v);

    return ret;
}

void uvc_video_id_remove(int id)
{
    pthread_mutex_lock(&mtx_v);
    if (_uvc_video_id_check(id)) {
        for (std::list<struct uvc_video*>::iterator i = lst_v.begin(); i != lst_v.end(); ++i) {
            struct uvc_video* l = *i;
            if (id == l->id) {
                pthread_mutex_destroy(&l->buffer_mutex);
                pthread_mutex_destroy(&l->user_mutex);
                free(l);
                lst_v.erase(i);
                break;
            }
        }
    }
    pthread_mutex_unlock(&mtx_v);
}

int uvc_video_id_get(unsigned int seq)
{
    int ret = -1;

    pthread_mutex_lock(&mtx_v);
    if (!lst_v.empty()) {
        unsigned int cnt = 0;
        for (std::list<struct uvc_video*>::iterator i = lst_v.begin(); i != lst_v.end(); ++i) {
            if (cnt++ == seq) {
                struct uvc_video* l = *i;
                ret = l->id;
                break;
            }
        }
    }
    pthread_mutex_unlock(&mtx_v);

    return ret;
}

static void uvc_gadget_pthread_exit(int id);

static int uvc_video_id_exit(int id)
{
    if (uvc_video_id_check(id)) {
        uvc_gadget_pthread_exit(id);
        uvc_video_id_remove(id);
        return 0;
    }

    return -1;
}

static int _uvc_video_id_exit_all()
{
    int ret = -1;

    pthread_mutex_lock(&mtx_v);
    if (!lst_v.empty()) {
        struct uvc_video* l = lst_v.front();
        pthread_mutex_unlock(&mtx_v);
        uvc_video_id_exit(l->id);
        pthread_mutex_lock(&mtx_v);
        ret = 0;
    }
    pthread_mutex_unlock(&mtx_v);

    return ret;
}

void uvc_video_id_exit_all()
{
    while (!_uvc_video_id_exit_all())
        continue;
}

static void _uvc_video_set_uvc_process(struct uvc_video* v, bool state)
{
    v->uvc_process = state;
}

void uvc_video_set_uvc_process(int id, bool state)
{
    pthread_mutex_lock(&mtx_v);
    if (_uvc_video_id_check(id)) {
        for (std::list<struct uvc_video*>::iterator i = lst_v.begin(); i != lst_v.end(); ++i) {
            struct uvc_video* l = *i;
            if (id == l->id) {
                _uvc_video_set_uvc_process(l, state);
                break;
            }
        }
    }
    pthread_mutex_unlock(&mtx_v);
}

static bool _uvc_video_get_uvc_process(struct uvc_video* v)
{
    return v->uvc_process;
}

bool uvc_video_get_uvc_process(int id)
{
    bool state = false;

    pthread_mutex_lock(&mtx_v);
    if (_uvc_video_id_check(id)) {
        for (std::list<struct uvc_video*>::iterator i = lst_v.begin(); i != lst_v.end(); ++i) {
            struct uvc_video* l = *i;
            if (id == l->id) {
                state = _uvc_video_get_uvc_process(l);
                break;
            }
        }
    }
    pthread_mutex_unlock(&mtx_v);

    return state;
}

pthread_t* uvc_video_get_uvc_pid(int id)
{
    pthread_t *tid = NULL;

    pthread_mutex_lock(&mtx_v);
    if (_uvc_video_id_check(id)) {
        for (std::list<struct uvc_video*>::iterator i = lst_v.begin(); i != lst_v.end(); ++i) {
            struct uvc_video* l = *i;
            if (id == l->id) {
                tid = &l->uvc_pid;
                break;
            }
        }
    }
    pthread_mutex_unlock(&mtx_v);

    return tid;
}

void uvc_video_join_uvc_pid(int id)
{
    pthread_mutex_lock(&mtx_v);
    if (_uvc_video_id_check(id)) {
        for (std::list<struct uvc_video*>::iterator i = lst_v.begin(); i != lst_v.end(); ++i) {
            struct uvc_video* l = *i;
            if (id == l->id) {
                if (l->uvc_pid) {
                    pthread_mutex_unlock(&mtx_v);
                    pthread_join(l->uvc_pid, NULL);
                    l->uvc_pid = 0;
                    pthread_mutex_lock(&mtx_v);
                }
            }
        }
    }
    pthread_mutex_unlock(&mtx_v);
}

static void uvc_gadget_pthread_exit(int id)
{
    while (!uvc_get_user_run_state(id))
        pthread_yield();
    uvc_set_user_run_state(false, id);
    uvc_video_join_uvc_pid(id);
}

static void _uvc_get_user_resolution(struct uvc_video *v, int* width, int* height);

static int _uvc_buffer_init(struct uvc_video *v)
{
    int i = 0;
    int ret = 0;
    struct uvc_buffer* buffer = NULL;
    int width, height;

    _uvc_get_user_resolution(v, &width, &height);

    pthread_mutex_lock(&v->buffer_mutex);

    v->uvc = new video_uvc();
    if (!v->uvc) {
        ret = -1;
        goto exit;
    }
    v->uvc->id = 0;
    v->uvc->video_id = v->id;
    v->uvc->run = 1;
    v->buffer_s = NULL;
    pthread_mutex_init(&v->uvc->write.mutex, NULL);
    pthread_mutex_init(&v->uvc->read.mutex, NULL);
    uvc_buffer_clear(&v->uvc->write);
    uvc_buffer_clear(&v->uvc->read);
    printf("UVC_BUFFER_NUM = %d\n", UVC_BUFFER_NUM);
    for (i = 0; i < UVC_BUFFER_NUM; i++) {
        buffer = uvc_buffer_create(width, height, v->id);
        if (!buffer) {
            ret = -1;
            goto exit;
        }
        uvc_buffer_push_back(&v->uvc->write, buffer);
    }
    _uvc_video_set_uvc_process(v, true);

exit:
    pthread_mutex_unlock(&v->buffer_mutex);
    return ret;
}

int uvc_buffer_init(int id)
{
    int ret = -1;
    pthread_mutex_lock(&mtx_v);
    if (_uvc_video_id_check(id)) {
        for (std::list<struct uvc_video*>::iterator i = lst_v.begin(); i != lst_v.end(); ++i) {
            struct uvc_video* l = *i;
            if (id == l->id) {
                ret = _uvc_buffer_init(l);
                break;
            }
        }
    }
    pthread_mutex_unlock(&mtx_v);

    return ret;
}

static void _uvc_buffer_deinit(struct uvc_video *v)
{
    pthread_mutex_lock(&v->buffer_mutex);
    if (v->uvc) {
        v->uvc->run = 0;
        _uvc_video_set_uvc_process(v, false);
        if (v->buffer_s)
            uvc_buffer_push_back(&v->uvc->write, v->buffer_s);
        uvc_buffer_destroy(&v->uvc->write);
        uvc_buffer_destroy(&v->uvc->read);
        delete v->uvc;
        v->uvc = NULL;
    }
    pthread_mutex_unlock(&v->buffer_mutex);
}

void uvc_buffer_deinit(int id)
{
    pthread_mutex_lock(&mtx_v);
    if (_uvc_video_id_check(id)) {
        for (std::list<struct uvc_video*>::iterator i = lst_v.begin(); i != lst_v.end(); ++i) {
            struct uvc_video* l = *i;
            if (id == l->id) {
                _uvc_buffer_deinit(l);
                break;
            }
        }
    }
    pthread_mutex_unlock(&mtx_v);
}

static bool _uvc_buffer_write_enable(struct uvc_video *v)
{
    bool ret = false;

    if (pthread_mutex_trylock(&v->buffer_mutex))
        return ret;
    if (v->uvc && uvc_buffer_front(&v->uvc->write))
        ret = true;
    pthread_mutex_unlock(&v->buffer_mutex);
    return ret;
}

bool uvc_buffer_write_enable(int id)
{
    bool ret = false;

    pthread_mutex_lock(&mtx_v);
    if (_uvc_video_id_check(id)) {
        for (std::list<struct uvc_video*>::iterator i = lst_v.begin(); i != lst_v.end(); ++i) {
            struct uvc_video* l = *i;
            if (id == l->id) {
                ret = _uvc_buffer_write_enable(l);
                break;
            }
        }
    }
    pthread_mutex_unlock(&mtx_v);

    return ret;
}

#define EX_MAX_LEN 65535
#define EX_DATA_LEN (EX_MAX_LEN - 2)
static void _uvc_buffer_write(struct uvc_video *v,
                              unsigned short stamp,
                              void* extra_data,
                              size_t extra_size,
                              void* data,
                              size_t size,
                              unsigned int fcc)
{
    const size_t cnt = extra_size / (EX_DATA_LEN + 1) + 1;
    pthread_mutex_lock(&v->buffer_mutex);
    if (v->uvc && data) {
        struct uvc_buffer* buffer = uvc_buffer_pop_front(&v->uvc->write);
        if (buffer && buffer->buffer) {
            if (buffer->total_size >= extra_size + size) {
                switch (fcc) {
                case V4L2_PIX_FMT_YUYV:
#if YUYV_AS_RAW
#ifdef USE_RK_MODULE
                    raw16_to_raw8(buffer->width, buffer->height, data, buffer->buffer);
#else
                    memcpy(buffer->buffer, data, size);
#endif
#else
                    NV12_to_YUYV(buffer->width, buffer->height, data, buffer->buffer);
#endif
                    break;
                case V4L2_PIX_FMT_MJPEG:
                    if (extra_data && buffer->total_size >= extra_size + size + 4 * cnt) {
                        size_t index = 4;// FF D8 FF E0
                        size_t len = *((unsigned char*)data + index);
                        len = len * 256 + *((unsigned char*)data + index + 1);
                        index = index + len;
                        memcpy(buffer->buffer, data, index);
                        size_t ind = index;
                        for (size_t i = 1; i < cnt; i++) {
                            memset((char*)buffer->buffer + ind, 0xFF, 1);
                            ind++;
                            memset((char*)buffer->buffer + ind, 0xE2, 1);
                            ind++;
                            memset((char*)buffer->buffer + ind, EX_MAX_LEN / 256, 1);
                            ind++;
                            memset((char*)buffer->buffer + ind, EX_MAX_LEN % 256, 1);
                            ind++;
                            memcpy((char*)buffer->buffer + ind,
                                   (char*)extra_data + (i - 1) * EX_DATA_LEN, EX_DATA_LEN);
                            ind += EX_DATA_LEN;
                        }
                        memset((char*)buffer->buffer + ind, 0xFF, 1);
                        ind ++;
                        memset((char*)buffer->buffer + ind, 0xE2, 1);
                        ind++;
                        memset((char*)buffer->buffer + ind,
                               (2 + extra_size - EX_DATA_LEN * (cnt - 1)) / 256, 1);
                        ind++;
                        memset((char*)buffer->buffer + ind,
                               (2 + extra_size - EX_DATA_LEN * (cnt - 1)) % 256, 1);
                        ind++;
                        memcpy((char*)buffer->buffer + ind,
                               (char*)extra_data + EX_DATA_LEN * (cnt - 1),
                               extra_size - EX_DATA_LEN * (cnt - 1));
                        ind += extra_size - EX_DATA_LEN * (cnt - 1);
                        memcpy((char*)buffer->buffer + ind,
                                (char*)data + index, size - index);
                        extra_size += 4 * cnt;
                    } else {
                        memcpy(buffer->buffer, data, size);
                    }
                    //memcpy((char*)buffer->buffer + size, &stamp, sizeof(stamp));
                    //size += sizeof(stamp);
                    break;
                case V4L2_PIX_FMT_H264:
                    if (extra_data && extra_size > 0)
                        memcpy(buffer->buffer, extra_data, extra_size);
                    if (extra_size >= 0)
                        memcpy((char*)buffer->buffer + extra_size, data, size);
                    break;
                }
                buffer->size = extra_size + size;
                uvc_buffer_push_back(&v->uvc->read, buffer);
            } else {
                uvc_buffer_push_back(&v->uvc->write, buffer);
            }
        }
    }
    pthread_mutex_unlock(&v->buffer_mutex);
}

void uvc_buffer_write(unsigned short stamp,
                      void* extra_data,
                      size_t extra_size,
                      void* data,
                      size_t size,
                      unsigned int fcc,
                      int id)
{
    pthread_mutex_lock(&mtx_v);
    if (_uvc_video_id_check(id)) {
        for (std::list<struct uvc_video*>::iterator i = lst_v.begin(); i != lst_v.end(); ++i) {
            struct uvc_video* l = *i;
            if (id == l->id) {
                _uvc_buffer_write(l, stamp, extra_data, extra_size, data, size, fcc);
                break;
            }
        }
    }
    pthread_mutex_unlock(&mtx_v);
}

static void _uvc_set_user_resolution(struct uvc_video *v, int width, int height)
{
    pthread_mutex_lock(&v->user_mutex);
    v->uvc_user.width = width;
    v->uvc_user.height = height;
    printf("uvc_user.width = %u, uvc_user.height = %u\n", v->uvc_user.width,
           v->uvc_user.height);
    pthread_mutex_unlock(&v->user_mutex);
}

void uvc_set_user_resolution(int width, int height, int id)
{
    pthread_mutex_lock(&mtx_v);
    if (_uvc_video_id_check(id)) {
        for (std::list<struct uvc_video*>::iterator i = lst_v.begin(); i != lst_v.end(); ++i) {
            struct uvc_video* l = *i;
            if (id == l->id) {
                _uvc_set_user_resolution(l, width, height);
                break;
            }
        }
    }
    pthread_mutex_unlock(&mtx_v);
}

static void _uvc_get_user_resolution(struct uvc_video *v, int* width, int* height)
{
    pthread_mutex_lock(&v->user_mutex);
    *width = v->uvc_user.width;
    *height = v->uvc_user.height;
    pthread_mutex_unlock(&v->user_mutex);
}

void uvc_get_user_resolution(int* width, int* height, int id)
{
    pthread_mutex_lock(&mtx_v);
    if (_uvc_video_id_check(id)) {
        for (std::list<struct uvc_video*>::iterator i = lst_v.begin(); i != lst_v.end(); ++i) {
            struct uvc_video* l = *i;
            if (id == l->id) {
                _uvc_get_user_resolution(l , width, height);
                break;
            }
        }
    }
    pthread_mutex_unlock(&mtx_v);
}

static bool _uvc_get_user_run_state(struct uvc_video *v)
{
    bool ret;

    pthread_mutex_lock(&v->user_mutex);
    ret = v->uvc_user.run;
    pthread_mutex_unlock(&v->user_mutex);

    return ret;
}

bool uvc_get_user_run_state(int id)
{
    bool state = false;

    pthread_mutex_lock(&mtx_v);
    if (_uvc_video_id_check(id)) {
        for (std::list<struct uvc_video*>::iterator i = lst_v.begin(); i != lst_v.end(); ++i) {
            struct uvc_video* l = *i;
            if (id == l->id) {
                state = _uvc_get_user_run_state(l);
                break;
            }
        }
    }
    pthread_mutex_unlock(&mtx_v);

    return state;
}

static void _uvc_set_user_run_state(struct uvc_video *v, bool state)
{
    pthread_mutex_lock(&v->user_mutex);
    v->uvc_user.run = state;
    pthread_mutex_unlock(&v->user_mutex);
}

void uvc_set_user_run_state(bool state, int id)
{
    pthread_mutex_lock(&mtx_v);
    if (_uvc_video_id_check(id)) {
        for (std::list<struct uvc_video*>::iterator i = lst_v.begin(); i != lst_v.end(); ++i) {
            struct uvc_video* l = *i;
            if (id == l->id) {
                _uvc_set_user_run_state(l, state);
                break;
            }
        }
    }
    pthread_mutex_unlock(&mtx_v);
}

static void _uvc_set_user_fcc(struct uvc_video *v, unsigned int fcc)
{
    v->uvc_user.fcc = fcc;
}

void uvc_set_user_fcc(unsigned int fcc, int id)
{
    pthread_mutex_lock(&mtx_v);
    if (_uvc_video_id_check(id)) {
        for (std::list<struct uvc_video*>::iterator i = lst_v.begin(); i != lst_v.end(); ++i) {
            struct uvc_video* l = *i;
            if (id == l->id) {
                _uvc_set_user_fcc(l, fcc);
                break;
            }
        }
    }
    pthread_mutex_unlock(&mtx_v);
}

static unsigned int _uvc_get_user_fcc(struct uvc_video *v)
{
    return v->uvc_user.fcc;
}

unsigned int uvc_get_user_fcc(int id)
{
    unsigned int fcc = 0;

    pthread_mutex_lock(&mtx_v);
    if (_uvc_video_id_check(id)) {
        for (std::list<struct uvc_video*>::iterator i = lst_v.begin(); i != lst_v.end(); ++i) {
            struct uvc_video* l = *i;
            if (id == l->id) {
                fcc = _uvc_get_user_fcc(l);
                break;
            }
        }
    }
    pthread_mutex_unlock(&mtx_v);

    return fcc;
}

static void _uvc_memset_uvc_user(struct uvc_video *v)
{
    memset(&v->uvc_user, 0, sizeof(struct uvc_user));
}

void uvc_memset_uvc_user(int id)
{
    pthread_mutex_lock(&mtx_v);
    if (_uvc_video_id_check(id)) {
        for (std::list<struct uvc_video*>::iterator i = lst_v.begin(); i != lst_v.end(); ++i) {
            struct uvc_video* l = *i;
            if (id == l->id) {
                _uvc_memset_uvc_user(l);
                break;
            }
        }
    }
    pthread_mutex_unlock(&mtx_v);
}

static bool _uvc_buffer_check(struct uvc_video* v, struct uvc_buffer* buffer)
{
    int width = 0, height = 0;

    _uvc_get_user_resolution(v, &width, &height);
    if (buffer->width == width && buffer->height == height)
        return true;
    else
        return false;
}

static void _uvc_user_fill_buffer(struct uvc_video *v, struct uvc_device *dev, struct v4l2_buffer *buf)
{
    struct uvc_buffer* buffer = NULL;

    while (!(buffer = uvc_buffer_front(&v->uvc->read)) && _uvc_get_user_run_state(v)) {
        pthread_mutex_unlock(&mtx_v);
        usleep(1000);
        pthread_mutex_lock(&mtx_v);
    }
    if (buffer) {
        if (!_uvc_buffer_check(v, buffer))
            return;
        if (_uvc_get_user_run_state(v) && _uvc_video_get_uvc_process(v)) {
            if (buf->length >= buffer->size && buffer->buffer) {
                buf->bytesused = buffer->size;
                memcpy(dev->mem[buf->index].start, buffer->buffer, buffer->size);
            }
        } else {
            buf->bytesused = buf->length;
        }
        buffer = uvc_buffer_pop_front(&v->uvc->read);
        if (!v->buffer_s) {
            v->buffer_s = buffer;
        } else {
            uvc_buffer_push_back(&v->uvc->write, v->buffer_s);
            v->buffer_s = buffer;
        }
    } else if (v->buffer_s) {
        if (!_uvc_buffer_check(v, v->buffer_s))
            return;
        if (_uvc_get_user_run_state(v) && _uvc_video_get_uvc_process(v)) {
            if (buf->length >= v->buffer_s->size && v->buffer_s->buffer) {
                buf->bytesused = v->buffer_s->size;
                memcpy(dev->mem[buf->index].start, v->buffer_s->buffer, v->buffer_s->size);
            }
        }
    } else {
        buf->bytesused = buf->length;
        memset(dev->mem[buf->index].start, 0, buf->length);
    }
}

void uvc_user_fill_buffer(struct uvc_device *dev, struct v4l2_buffer *buf, int id)
{
    pthread_mutex_lock(&mtx_v);
    if (_uvc_video_id_check(id)) {
        for (std::list<struct uvc_video*>::iterator i = lst_v.begin(); i != lst_v.end(); ++i) {
            struct uvc_video* l = *i;
            if (id == l->id) {
                _uvc_user_fill_buffer(l, dev, buf);
                break;
            }
        }
    }
    pthread_mutex_unlock(&mtx_v);
}
