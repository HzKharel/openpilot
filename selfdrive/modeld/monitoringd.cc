#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <cassert>

#include "common/visionbuf.h"
#include "common/visionipc.h"
#include "common/swaglog.h"

#include "models/monitoring.h"

#ifndef PATH_MAX
#include <linux/limits.h>
#endif


volatile sig_atomic_t do_exit = 0;

static void set_do_exit(int sig) {
  do_exit = 1;
}

int main(int argc, char **argv) {
  int err;
  set_realtime_priority(1);

  // messaging
  Context *msg_context = Context::create();
  PubSocket *monitoring_sock = PubSocket::create(msg_context, "driverMonitoring");

  // init the models
  MonitoringState monitoring;
  monitoring_init(&monitoring);

  // loop
  VisionStream stream;
  while (!do_exit) {
    VisionStreamBufs buf_info;
    err = visionstream_init(&stream, VISION_STREAM_YUV_FRONT, true, &buf_info);
    if (err) {
      printf("visionstream connect fail\n");
      usleep(100000);
      continue;
    }
    LOGW("connected with buffer size: %d", buf_info.buf_len);

    double last = 0;
    while (!do_exit) {
      VIPCBuf *buf;
      VIPCBufExtra extra;
      buf = visionstream_get(&stream, &extra);
      if (buf == NULL) {
        printf("visionstream get failed\n");
        break;
      }
      //printf("frame_id: %d %dx%d\n", extra.frame_id, buf_info.width, buf_info.height);

      double t1 = millis_since_boot();

      MonitoringResult res = monitoring_eval_frame(&monitoring, buf->addr, buf_info.width, buf_info.height);

      double t2 = millis_since_boot();

      // send dm packet
      monitoring_publish(monitoring_sock, extra.frame_id, res);

      LOGD("monitoring process: %.2fms, from last %.2fms", t2-t1, t1-last);
      last = t1;
    }

  }

  visionstream_destroy(&stream);

  delete monitoring_sock;
  monitoring_free(&monitoring);

  return 0;
}