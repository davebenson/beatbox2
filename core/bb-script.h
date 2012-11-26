#ifndef __BB_SCRIPT_H_
#define __BB_SCRIPT_H_

#include "bb-score.h"

BbScore *bb_script_execute (const char *filename,
                            BbScoreRenderConfig *config,
                            GError    **error);

#endif
