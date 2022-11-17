#pragma once
class Daemon
{
public:
   static void start();
   static void daemonize();
   static void initStateThread();
};