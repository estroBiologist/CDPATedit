#include "CDPAT.h"

cdpat::EventsData& cdpat::Action::getEventsData(Pattern& pattern) {
	return pattern.events;
}
