/* kb_distort.h aka the thumb enabler :-)

   this program slightly alter keyboard shape to take into account
   different biases:
   - users won't go near screen edges by fear of "swiping" to app screen
   - 4.5 in. screen makes some area of the keyboard difficult to reach
     with your thumb in case of one-handed operation
 */

#ifndef KB_DISTORT_H
#define KB_DISTORT_H

#include <QHash>
#include <QString>

#include "curve_match.h"

void kb_distort_cancel(QHash<QString, Key> &keys);
void kb_distort(QHash<QString, Key> &keys, Params &params, float scale);

#endif /* KB_DISTORT_H */
