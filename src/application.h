#pragma once
#include "stable.h"

#include <vector>
#include <memory>

struct application {
	struct config {
		HINSTANCE instanceHandle;
		bool showCommand;

		std::vector<int> displays;
		float zoom = 1;
		//RECT rect;

      bool showCaptureFrameRate = false;
	};

	application(config&& cfg);

	int run();

private:
	struct internal;
	struct internal_deleter {
		void operator() (internal*);
	};
	using internal_ptr = std::unique_ptr<internal, internal_deleter>;

	internal_ptr ip;
};
