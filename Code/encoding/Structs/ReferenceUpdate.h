#pragma once

// Retired: upstream removed ReferenceUpdate when action events moved into their own messages
// (RequestActionEvents / NotifyActionEvents) and the move requests started carrying Movement
// directly. Kept as an empty shim here because this working tree cannot delete files; remove
// this header and ReferenceUpdate.cpp from the tree once the port lands.
