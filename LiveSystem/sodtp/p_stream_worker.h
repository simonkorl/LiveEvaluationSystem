#ifndef P_STREAM_WORKER_H
#define P_STREAM_WORKER_H

#include <vector>
#include <thread>
#include "p_sodtp_jitter.h"
#include "p_sdl_play.h"
#include "p_decode_video.h"
#include "../util/util_log.h"
#include "p_decode_audio.h"

using namespace std;

class StreamWorker
{
public:
    StreamWorker() {}
    ~StreamWorker() {}
public:
  JitterBuffer        jbuffer; // 用于缓存所有的数据块

  SDLPlay             splay; // 处理视频帧的更新与播放
  AudioPlayer         *aplayer = NULL; // 处理音频播放的对象指针，所有权为 main()
  SDL_Rect            rect;

  vector<thread*>     thds;
  thread              *thd_conn;
  const char          *host;
  const char          *port;
  const char          *path;
};


// Check the state of jitter buffer and network.
// And then take actions according to the state.
// This function creates thread to play video and audio stream
void stream_working(struct ev_loop *loop, ev_signal *w, int revents) {
    timeMainPlayer.evalTime("p","stream_working_Started");
    StreamWorker *worker = (StreamWorker*)w->data; // owner main()

    bool found = false;
    printf("a new signal.\n");


    // lock the jptrs.
    scoped_lock lock(worker->jbuffer.mtx);
    auto it = worker->jbuffer.jptrs.begin();
    fprintf(stderr, "searching signal source, jitter queue number %lu\n", worker->jbuffer.jptrs.size());
    while (it != worker->jbuffer.jptrs.end()) {
        if (((*it)->state & SodtpJitter::STATE_INIT) == SodtpJitter::STATE_INIT &&
            (*it)->get_work_thread() == NULL) {
          // TODO: WARNING: temporary implementation
          // Assume all video streams is stream_id == 0
          // and assume all audio streams is stream_id == 1
          if((*it)->stream_id == 0) {
            // if it is a video stream, start video viewer
            Print2FileInfo("(p)启动video_viewer4线程处");
            timeMainPlayer.evalTime("p", "video_viewer4Start");
            fprintf(stderr, "[WARNING] new video thread!!!!!\n");
            thread *pthd =
                new thread(video_viewer4, *it, &worker->splay, worker->path);
            (*it)->set_work_thread(pthd);
            worker->thds.push_back(pthd);
            found = true;
          } else if((*it)->stream_id == 1) {
            // if it is an audio stream, start audio viewer
            Print2FileInfo("(p)启动audio_viewer线程处");
            timeMainPlayer.evalTime("p", "audio_viewer Start");
            fprintf(stderr, "[WARNING] new audio thread!!!!!\n");
            fprintf(stderr, "[INFO]worker->aplayer: %p\n", worker->aplayer);
            thread *pthd =
                new thread(audio_viewer, *it, worker->aplayer);
            (*it)->set_work_thread(pthd);
            worker->thds.push_back(pthd);
            found = true;
          }
        }
        // We DO NOT kill thread.
        // Thread should be stopped by itself due to the 'FIN' flag in block.
        else if (((*it)->state & SodtpJitter::STATE_CLOSE) == SodtpJitter::STATE_CLOSE) {
            // thread *pthd = (*it)->get_work_thread();
            it = worker->jbuffer.jptrs.erase(it);
            printf("thread: state close\n");
            found = true;
        }
        else {
            ++it;
        }

    }

    if (found == false) {
        printf("Warning! Unable to find target.\n");
        printf("It could be the signal of connection closed. Exit now\n");

        // stop playing.
        // ev_signal_stop(loop, w);
        //
        // ev_break (EV_A_ EVBREAK_ALL);
        ev_break(EV_A_ EVBREAK_ONE);
    }
    // while (jbuffer.jptrs.size() > 0);
    if (worker->jbuffer.jptrs.size() == 0) {
        printf("Warning! No Stream left, exit now.\n");
        // stop playing.
        // ev_signal_stop(loop, w);
        //
        // ev_break (EV_A_ EVBREAK_ALL);
        ev_break(EV_A_ EVBREAK_ONE);
    }
}





#endif // P_STREAM_WORKER_H
