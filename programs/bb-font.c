#include <stdlib.h>     /* for bsearch() */
#include "bb-font.h"

/* note must be sorted */
typedef struct _CharInfo CharInfo;
struct _CharInfo
{
  guint8 c;
  guint8 width;
  guint8 data[7];
};
#define MAKE_DATA(a,b,c,d,e,f,g,h) \
                    ( ((a)<<7) \
                    | ((b)<<6) \
                    | ((c)<<5) \
                    | ((d)<<4) \
                    | ((e)<<3) \
                    | ((f)<<2) \
                    | ((g)<<1) \
                    | ((h)<<0) )

/* NOTE: must be sorted */
static CharInfo char_infos[] =
{
  { ' ', 3, {
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { '!', 1, {
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { '"', 3, {
  MAKE_DATA(1,0,1,0,0,0,0,0),
  MAKE_DATA(1,0,1,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { '#', 5, {
  MAKE_DATA(0,1,0,1,0,0,0,0),
  MAKE_DATA(1,1,1,1,1,0,0,0),
  MAKE_DATA(0,1,0,1,0,0,0,0),
  MAKE_DATA(1,1,1,1,1,0,0,0),
  MAKE_DATA(0,1,0,1,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { '$', 4, {
  MAKE_DATA(0,0,1,0,0,0,0,0),
  MAKE_DATA(0,1,1,1,0,0,0,0),
  MAKE_DATA(1,1,0,0,0,0,0,0),
  MAKE_DATA(0,0,1,1,0,0,0,0),
  MAKE_DATA(1,1,1,0,0,0,0,0),
  MAKE_DATA(0,0,1,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { '%', 5, {
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(0,0,0,1,0,0,0,0),
  MAKE_DATA(0,0,1,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,1,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { '&', 5, {
  MAKE_DATA(0,1,1,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(0,1,1,0,1,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(0,1,1,0,1,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { '(', 2, {
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { '*', 3, {
  MAKE_DATA(1,0,1,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(1,0,1,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { '+', 3, {
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(1,1,1,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { ',', 2, {
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { '-', 3, {
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(1,1,1,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { '.', 1, {
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { '/', 5, {
  MAKE_DATA(0,0,0,0,1,0,0,0),
  MAKE_DATA(0,0,0,1,0,0,0,0),
  MAKE_DATA(0,0,1,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { '0', 4, {
  MAKE_DATA(0,1,1,0,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(0,1,1,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { '1', 2, {
  MAKE_DATA(1,1,0,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { '2', 4, {
  MAKE_DATA(1,1,1,0,0,0,0,0),
  MAKE_DATA(0,0,0,1,0,0,0,0),
  MAKE_DATA(0,1,1,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(1,1,1,1,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { '3', 4, {
  MAKE_DATA(1,1,1,0,0,0,0,0),
  MAKE_DATA(0,0,0,1,0,0,0,0),
  MAKE_DATA(0,1,1,0,0,0,0,0),
  MAKE_DATA(0,0,0,1,0,0,0,0),
  MAKE_DATA(1,1,1,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { '4', 4, {
  MAKE_DATA(0,0,1,0,0,0,0,0),
  MAKE_DATA(0,1,1,0,0,0,0,0),
  MAKE_DATA(1,0,1,0,0,0,0,0),
  MAKE_DATA(1,1,1,1,0,0,0,0),
  MAKE_DATA(0,0,1,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { '5', 4, {
  MAKE_DATA(1,1,1,1,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(1,1,1,0,0,0,0,0),
  MAKE_DATA(0,0,0,1,0,0,0,0),
  MAKE_DATA(1,1,1,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { '6', 4, {
  MAKE_DATA(0,1,1,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(1,1,1,0,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(0,1,1,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { '7', 4, {
  MAKE_DATA(1,1,1,1,0,0,0,0),
  MAKE_DATA(0,0,0,1,0,0,0,0),
  MAKE_DATA(0,0,1,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { '8', 4, {
  MAKE_DATA(0,1,1,0,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(0,1,1,0,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(0,1,1,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { '9', 4, {
  MAKE_DATA(0,1,1,0,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(0,1,1,1,0,0,0,0),
  MAKE_DATA(0,0,0,1,0,0,0,0),
  MAKE_DATA(0,1,1,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { ';', 1, {
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { '<', 3, {
  MAKE_DATA(0,0,1,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(0,0,1,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { '=', 3, {
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(1,1,1,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(1,1,1,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { '>', 3, {
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(0,0,1,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { '?', 4, {
  MAKE_DATA(1,1,1,0,0,0,0,0),
  MAKE_DATA(0,0,0,1,0,0,0,0),
  MAKE_DATA(0,1,1,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { '@', 5, {
  MAKE_DATA(0,1,1,1,0,0,0,0),
  MAKE_DATA(1,0,0,0,1,0,0,0),
  MAKE_DATA(1,0,1,1,1,0,0,0),
  MAKE_DATA(1,0,1,0,1,0,0,0),
  MAKE_DATA(0,1,1,1,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'A', 4, {
  MAKE_DATA(0,1,1,0,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,1,1,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'B', 4, {
  MAKE_DATA(1,1,1,0,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,1,1,0,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,1,1,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'C', 3, {
  MAKE_DATA(0,1,1,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(0,1,1,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'D', 4, {
  MAKE_DATA(1,1,1,0,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,1,1,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'E', 3, {
  MAKE_DATA(1,1,1,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(1,1,1,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(1,1,1,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'F', 3, {
  MAKE_DATA(1,1,1,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(1,1,1,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'G', 4, {
  MAKE_DATA(0,1,1,1,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(1,0,1,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(0,1,1,1,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'H', 4, {
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,1,1,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'I', 3, {
  MAKE_DATA(1,1,1,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(1,1,1,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'J', 4, {
  MAKE_DATA(0,0,1,1,0,0,0,0),
  MAKE_DATA(0,0,0,1,0,0,0,0),
  MAKE_DATA(0,0,0,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(0,1,1,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'K', 4, {
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,0,1,0,0,0,0,0),
  MAKE_DATA(1,1,0,0,0,0,0,0),
  MAKE_DATA(1,0,1,0,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'L', 3, {
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(1,1,1,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'M', 5, {
  MAKE_DATA(1,0,0,0,1,0,0,0),
  MAKE_DATA(1,1,0,1,1,0,0,0),
  MAKE_DATA(1,0,1,0,1,0,0,0),
  MAKE_DATA(1,0,0,0,1,0,0,0),
  MAKE_DATA(1,0,0,0,1,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'N', 4, {
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,1,0,1,0,0,0,0),
  MAKE_DATA(1,0,1,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'O', 4, {
  MAKE_DATA(0,1,1,0,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(0,1,1,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'P', 4, {
  MAKE_DATA(1,1,1,0,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,1,1,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'Q', 4, {
  MAKE_DATA(0,1,1,0,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(0,1,1,0,0,0,0,0),
  MAKE_DATA(0,0,0,1,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'R', 4, {
  MAKE_DATA(1,1,1,0,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,1,1,0,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'S', 4, {
  MAKE_DATA(0,1,1,1,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(0,1,1,0,0,0,0,0),
  MAKE_DATA(0,0,0,1,0,0,0,0),
  MAKE_DATA(1,1,1,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'T', 3, {
  MAKE_DATA(1,1,1,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'U', 4, {
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(0,1,1,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'V', 4, {
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,0,1,0,0,0,0,0),
  MAKE_DATA(1,0,1,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'W', 5, {
  MAKE_DATA(1,0,0,0,1,0,0,0),
  MAKE_DATA(1,0,1,0,1,0,0,0),
  MAKE_DATA(1,0,1,0,1,0,0,0),
  MAKE_DATA(1,0,1,0,1,0,0,0),
  MAKE_DATA(0,1,0,1,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'X', 4, {
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(0,1,1,0,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'Y', 4, {
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(0,1,1,1,0,0,0,0),
  MAKE_DATA(0,0,0,1,0,0,0,0),
  MAKE_DATA(0,1,1,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'Z', 3, {
  MAKE_DATA(1,1,1,0,0,0,0,0),
  MAKE_DATA(0,0,1,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(1,1,1,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { '[', 2, {
  MAKE_DATA(1,1,0,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(1,1,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { ']', 2, {
  MAKE_DATA(1,1,0,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(1,1,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { '^', 3, {
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(1,0,1,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { '_', 4, {
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(1,1,1,1,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'a', 4, {
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(0,1,1,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(0,1,1,1,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'b', 4, {
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(1,1,1,0,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,1,1,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'c', 3, {
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(0,1,1,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(0,1,1,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'd', 4, {
  MAKE_DATA(0,0,0,1,0,0,0,0),
  MAKE_DATA(0,1,1,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(0,1,1,1,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'e', 4, {
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(0,1,1,0,0,0,0,0),
  MAKE_DATA(1,0,1,1,0,0,0,0),
  MAKE_DATA(1,1,0,0,0,0,0,0),
  MAKE_DATA(0,1,1,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'f', 3, {
  MAKE_DATA(0,0,1,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(1,1,1,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'g', 4, {
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(0,1,1,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(0,1,1,1,0,0,0,0),
  MAKE_DATA(0,0,0,1,0,0,0,0), 
  MAKE_DATA(0,1,1,0,0,0,0,0) } },
  { 'h', 4, {
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(1,1,1,0,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'i', 1, {
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'j', 2, {
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0), 
  MAKE_DATA(1,0,0,0,0,0,0,0) } },
  { 'k', 4, {
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,0,1,0,0,0,0,0),
  MAKE_DATA(1,1,1,0,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'l', 1, {
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'm', 5, {
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(1,1,1,1,0,0,0,0),
  MAKE_DATA(1,0,1,0,1,0,0,0),
  MAKE_DATA(1,0,1,0,1,0,0,0),
  MAKE_DATA(1,0,1,0,1,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'n', 4, {
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(1,1,1,0,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'o', 4, {
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(0,1,1,0,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(0,1,1,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'p', 4, {
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(1,1,1,0,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,1,1,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'q', 4, {
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(0,1,1,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(0,1,1,1,0,0,0,0),
  MAKE_DATA(0,0,0,1,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'r', 3, {
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(1,0,1,0,0,0,0,0),
  MAKE_DATA(1,1,0,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 's', 4, {
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(0,1,1,1,0,0,0,0),
  MAKE_DATA(1,1,0,0,0,0,0,0),
  MAKE_DATA(0,0,1,1,0,0,0,0),
  MAKE_DATA(1,1,1,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 't', 3, {
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(1,1,1,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(0,0,1,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'u', 4, {
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(0,1,1,1,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'v', 4, {
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,0,1,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'w', 5, {
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(1,0,1,0,1,0,0,0),
  MAKE_DATA(1,0,1,0,1,0,0,0),
  MAKE_DATA(0,1,0,1,0,0,0,0),
  MAKE_DATA(0,1,0,1,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'x', 3, {
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(1,0,1,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(1,0,1,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { 'y', 4, {
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(1,0,0,1,0,0,0,0),
  MAKE_DATA(0,1,1,1,0,0,0,0),
  MAKE_DATA(0,0,0,1,0,0,0,0), 
  MAKE_DATA(0,0,1,0,0,0,0,0) } },
  { 'z', 4, {
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(1,1,1,1,0,0,0,0),
  MAKE_DATA(0,0,1,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(1,1,1,1,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { '{', 3, {
  MAKE_DATA(0,1,1,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(1,0,0,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(0,1,1,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { '}', 3, {
  MAKE_DATA(1,1,0,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(0,0,1,0,0,0,0,0),
  MAKE_DATA(0,1,0,0,0,0,0,0),
  MAKE_DATA(1,1,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
  { '~', 4, {
  MAKE_DATA(0,1,0,1,0,0,0,0),
  MAKE_DATA(1,0,1,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0),
  MAKE_DATA(0,0,0,0,0,0,0,0), 
  MAKE_DATA(0,0,0,0,0,0,0,0) } },
};

unsigned bb_font_get_height (void)
{
  return 7;
}

unsigned bb_font_get_ascent (void)
{
  return 5;
}
unsigned bb_font_get_descent (void)
{
  return 2;
}
static int
compare_char_against_char_info (gconstpointer a,
                                gconstpointer b)
{
  guint8 c = GPOINTER_TO_UINT (a);
  const CharInfo *ci = b;
  return (c < ci->c) ? -1
       : (c > ci->c) ? 1
       : 0;
}

#define get_char_info(c) \
  (const CharInfo *) \
      bsearch (GUINT_TO_POINTER ((guint)c), \
               char_infos, G_N_ELEMENTS (char_infos), \
               sizeof (CharInfo), compare_char_against_char_info)

unsigned bb_font_get_text_width (const char *text)
{
  unsigned width = 0;
  while (*text)
    {
      const CharInfo *ci = get_char_info (*text);
      if (ci == NULL)
        g_error ("font has no entry '%c'", *text);
      width += ci->width + 1;
      text++;
    }
  return width;
}

void bb_font_draw_text (guint8 *img_data,
                     unsigned rowstride,
                     const guint8 *rgb,
                     const char *text)
{
  while (*text)
    {
      const CharInfo *ci = get_char_info (*text);
      guint i;
      if (ci == NULL)
        g_error ("font has no entry '%c'", *text);
      for (i = 0; i < 7; i++)
        {
          guint8 *row_at = img_data + i * rowstride;
          guint8 mask = 0x80;
          guint x;
          for (x = 0; x < ci->width; x++)
            {
              if (mask & ci->data[i])
                {
                  *row_at++ = rgb[0];
                  *row_at++ = rgb[1];
                  *row_at++ = rgb[2];
                }
              else
                row_at += 3;
              mask >>= 1;
            }
        }
      img_data += (ci->width + 1) * 3;
      text++;
    }
}
