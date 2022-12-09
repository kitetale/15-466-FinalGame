#include "GP22IntroMode.hpp"

#include "GL.hpp"
#include "gl_compile_program.hpp"
#include "gl_errors.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtx/transform.hpp>

#include <algorithm>
#include <random>
#include <fstream>
#include <array>
#include <unordered_map>

namespace {
	//letters:
	[[maybe_unused]] const glm::u8vec4 FgFill1 = glm::u8vec4(0x9a, 0x22, 0x6d, 0xff);
	[[maybe_unused]] const glm::u8vec4 FgFill2 = glm::u8vec4(0x59, 0x00, 0x3c, 0xff);
	[[maybe_unused]] const glm::u8vec4 FgStroke1 = glm::u8vec4(0xcf, 0x49, 0x99, 0xff);
	[[maybe_unused]] const glm::u8vec4 FgStroke2 = glm::u8vec4(0x59, 0x00, 0x3c, 0xff);

	//capsule:
	[[maybe_unused]] const glm::u8vec4 Fg = glm::u8vec4(0xd4, 0xcf, 0xea, 0xff);

	//stripes:
	const glm::u8vec4 Bg1 = glm::u8vec4(0xe2, 0xe2, 0xfe, 0xff);
	const glm::u8vec4 Bg2 = glm::u8vec4(0xe7, 0xdb, 0xfc, 0xff);
}


static void stroke(std::vector< GP22IntroMode::Vertex > *attribs_,
		std::vector< glm::vec2 > const &path, std::vector< float > const &param, float r,
		glm::u8vec4 col_begin, std::vector< std::pair< float, glm::u8vec4 > > const &col_mid, glm::u8vec4 col_end) {
	assert(attribs_);
	auto &attribs = *attribs_;

	assert(param.size() == path.size());

	if (path.size() < 2) return;

	glm::vec2 min = glm::vec2(std::numeric_limits< float >::infinity());
	glm::vec2 max = glm::vec2(-std::numeric_limits< float >::infinity());

	std::vector< std::pair< float, glm::u8vec4 > > param_col;
	param_col.reserve(col_mid.size() + 2);
	param_col.emplace_back(0.0f, col_begin);
	param_col.insert(param_col.end(), col_mid.begin(), col_mid.end());
	param_col.emplace_back(param.back(), col_end);

	uint32_t pci = 0;

	bool first = true;
	auto emit = [&](glm::vec2 const &vert, float p) {
		while (pci + 1 < param_col.size() && param_col[pci+1].first <= p) ++pci;

		float amt = (p - param_col[pci].first) / (param_col[pci+1].first - param_col[pci].first);
		glm::u8vec4 col = glm::u8vec4(
			glm::mix(glm::vec4(param_col[pci].second), glm::vec4(param_col[pci+1].second), amt)
		);

		min = glm::min(min, vert);
		max = glm::max(max, vert);
		if (first && !attribs.empty()) attribs.emplace_back(attribs.back());
		attribs.emplace_back(vert, col);
		if (first && attribs.size() != 1) attribs.emplace_back(attribs.back());
		first = false;
	};

	//mitered line strip:
	glm::vec2 prev_along;
	{
		glm::vec2 along = glm::normalize(path[1] - path[0]);
		glm::vec2 perp = glm::vec2(-along.y, along.x);
		emit(path[0] - r * perp, param[0]);
		emit(path[0] + r * perp, param[0]);
		prev_along = along;
	}
	for (uint32_t i = 1; i + 1 < path.size(); ++i) {
		glm::vec2 along = glm::normalize(path[i+1] - path[i]);
		glm::vec2 perp = glm::vec2(-along.y, along.x);

		glm::vec2 avg_along = glm::normalize(along + prev_along);
		glm::vec2 avg_perp = glm::vec2(-avg_along.y, avg_along.x);

		float r_ = r / glm::max(glm::dot(avg_perp, perp), 0.5f);

		emit(path[i] - r_ * avg_perp, param[i]);
		emit(path[i] + r_ * avg_perp, param[i]);

		prev_along = along;
	}
	{
		glm::vec2 along = glm::normalize(path.back() - path[path.size()-2]);
		glm::vec2 perp = glm::vec2(-along.y, along.x);
		emit(path.back() - r * perp, param.back());
		emit(path.back() + r * perp, param.back());
	}
}

static std::mt19937 mt;

GP22IntroMode::LetterPath::LetterPath() {
	ofs_init = mt() / float(mt.max()) * 2.0f * float(M_PI);
	ofs_K = mt() / float(mt.max()) * 2.0f * float(M_PI);
}

void GP22IntroMode::LetterPath::compute(std::vector< Vertex > *attribs, float t) const {
	std::vector< glm::vec2 > path = base;

	//float vel_K = (std::sin(t * 10.0f + ofs_K + 1.0f) * 0.5f + 0.5f) * (0.7f - 0.3f) + 0.3f;
	//float vel_init = (std::sin(t * 7.0f + ofs_init + 2.0f) * 0.5f + 0.5f) * (10.0f - 6.0f) + 6.0f;
	float vel_K = (std::sin(1.0f * 10.0f + ofs_K + 1.0f) * 0.5f + 0.5f) * (0.7f - 0.3f) + 0.3f;
	float vel_init = (std::sin(1.0f * 7.0f + ofs_init + 2.0f) * 0.5f + 0.5f) * (7.0f - 5.0f) + 5.0f;

	//extend path out into a spiral somehow:
	glm::vec2 min = glm::vec2(std::numeric_limits< float >::infinity());
	glm::vec2 max = glm::vec2(-std::numeric_limits< float >::infinity());
	for (auto const &v : path) {
		min = glm::min(min, v);
		max = glm::max(max, v);
	}
	//make the center of the spiral "center":
	glm::vec2 center = 0.5f * (max + min);

	//spring thing:
	glm::vec2 at = path.back();
	glm::vec2 vel = vel_init * glm::normalize(path.back() - path[path.size()-2]);

	float r = glm::length(path.back() - center);
	float ang = std::atan2(path.back().y - center.y, path.back().x - center.x);

	for (uint32_t step = 0; step < 200; ++step) {
		r += 0.1f;
		ang -= 0.1f;
		glm::vec2 target = r * glm::vec2(std::cos(ang), std::sin(ang)) + center;
		vel *= std::pow(0.5f, 1.0f / 2000.0f);
		vel += vel_K * (target - at);
		at += 0.1f * vel;

		path.emplace_back(at);
	}

	std::vector< float > len;
	len.reserve(path.size());
	double total = 0.0;
	len.emplace_back(float(total));
	for (uint32_t i = 1; i < path.size(); ++i) {
		total += glm::length(path[i] - path[i-1]);
		len.emplace_back(float(total));
	}
	float core = len[base.size()-1];

	constexpr float VEL = 20.0f;

	float front = (strike - t) * VEL;
	float begin1 = std::max(front, 0.0f);
	float begin0 = begin1 - 0.01f * VEL;
	float begin2 = std::max(front + 0.05f * VEL, 0.0f);

	float end1 = std::max(core, front + 0.1f * VEL);
	float end0 = std::max(core, front + 0.5f * VEL);

	stroke(attribs, path, len, 0.16f, glm::u8vec4(0x00, 0x00, 0x00, 0x00), {
		std::make_pair(begin0, glm::u8vec4(FgFill1.r, FgFill1.g, FgFill1.b, 0x00)),
		std::make_pair(begin1, glm::u8vec4(FgFill1.r, FgFill1.g, FgFill1.b, 0xff)),
		std::make_pair(begin2, glm::u8vec4(FgFill1.r, FgFill1.g, FgFill1.b, 0xff)),
		std::make_pair(end1, glm::u8vec4(FgFill2.r, FgFill2.g, FgFill2.b, 0xff)),
		std::make_pair(end0, glm::u8vec4(FgFill2.r, FgFill2.g, FgFill2.b, 0x00))
	}, glm::u8vec4(0x00, 0x00, 0x00, 0x00) );
}

GP22IntroMode::GP22IntroMode(std::shared_ptr< Mode > const &next_mode_) : next_mode(next_mode_) {
	float bpm = 140.0f;
	{ // ------ music ------
		std::vector< float > data(10 * 48000, 0.0f);

		//triangle wave (lightly phase-modulated):
		[[maybe_unused]] auto triangle = [](float t) -> float {
			float pm = std::sin(t * 0.999f * 2.0f * float(M_PI));
			t += 0.1f * pm;
			t += 0.25f;
			return std::abs(4.0f * (t - std::floor(t)) - 2.0f) - 1.0f;
		};
		//sine wave (lightly phase-modulated + sub-bass):
		[[maybe_unused]] auto sine = [](float t) -> float {
			float pm = std::sin(t * 1.01f * 2.0f * float(M_PI));
			float sub = std::sin(0.5f * t * 2.0f * float(M_PI));
			return 0.5f * std::sin((t + 0.3f * pm) * 2.0f * float(M_PI)) + 0.5f * sub;
		};

		//square wave (lightly pulse-width-modulated):
		[[maybe_unused]] auto square = [](float t) -> float {
			float w = 0.5f + 0.1f * std::sin(t * 0.1f);
			return (t - std::floor(t) > w ? 1.0f : -1.0f);
		};

		constexpr float Attack  = 0.02f;
		constexpr float Decay   = 0.1f;
		constexpr float Sustain = 0.8f;
		constexpr float Release = 0.2f;

		//play synth at given time for given length at given frequency with given wave:
		auto tone = [&](float start, float len, float hz, float vol, std::function< float(float) > const &wave){
			int32_t begin = int32_t(start * 48000);
			int32_t end = begin + int32_t((len + Release) * 48000);

			for (int32_t sample = begin; sample < end; ++sample) {
				float t = (sample - begin + 0.5f) / 48000.0f;

				//envelope value:
				float env;
				if (t < Attack) env = t / Attack;
				else if (t < Attack + Decay) env = ((t - Attack) / Decay) * (Sustain - 1.0f) + 1.0f;
				else if (t <= len) env = Sustain;
				else env = std::min(1.0f, (t - len) / Release) * (0.0f - Sustain) + Sustain;

				//simple, single-oscillator synth:
				float osc1 = wave(t * hz);

				data[sample] += vol * env * osc1;
			}
		};

		//midi note number to frequency (based on A4 being 440hz)
		auto midi2hz = [](float midi) -> float {
			return 440.0f * std::exp2( (midi - 69.0f) / 12.0f );
		};

		[[maybe_unused]] auto C = [](int32_t oct) { return 12.0f + 12.0f * oct; };
		[[maybe_unused]] auto Cs= [](int32_t oct) { return 13.0f + 12.0f * oct; };
		[[maybe_unused]] auto D = [](int32_t oct) { return 14.0f + 12.0f * oct; };
		[[maybe_unused]] auto Ds= [](int32_t oct) { return 15.0f + 12.0f * oct; };
		[[maybe_unused]] auto E = [](int32_t oct) { return 16.0f + 12.0f * oct; };
		[[maybe_unused]] auto F = [](int32_t oct) { return 17.0f + 12.0f * oct; };
		[[maybe_unused]] auto Fs= [](int32_t oct) { return 18.0f + 12.0f * oct; };
		[[maybe_unused]] auto G = [](int32_t oct) { return 19.0f + 12.0f * oct; };
		[[maybe_unused]] auto Gs= [](int32_t oct) { return 20.0f + 12.0f * oct; };
		[[maybe_unused]] auto A = [](int32_t oct) { return 21.0f + 12.0f * oct; };
		[[maybe_unused]] auto As= [](int32_t oct) { return 22.0f + 12.0f * oct; };
		[[maybe_unused]] auto B = [](int32_t oct) { return 23.0f + 12.0f * oct; };
		

		auto tones = [&]( std::function< float(float) > const &wave, float hz, float step, std::string const &score ) {
			for (uint32_t begin = 0; begin < score.size(); /* later */) {
				if (score[begin] == '#' || score[begin] == 'o') {
					uint32_t end = begin + 1;
					while (end < score.size() && score[end] == '=') ++end;
					float vol = (score[begin] == '#' ? 0.4f : 0.2f);
					tone( begin * step, (end - begin - 0.5f) * step, hz, vol, wave );
					begin = end;
				} else {
					++begin;
				}
			}
		};

		//composed quickly in beepbox:
		//https://www.beepbox.co/#9n31s4k1l00e00t16a7g0fj07r1i0o432T0v1u22f0q0w10l5d08w4h1E1b8T1v5u01f10l7q8121d35AbF6B8Q28c0Pb745E179T1v1u83f0q8z10q5231d03AbF6B2Q0572P9995E2b273T4v1uf0f0q011z6666ji8k8k3jSBKSJJAArriiiiii07JCABrzrrrrrrr00YrkqHrsrrrrjr005zrAqzrjzrrqr1jRjrqGGrrzsrsA099ijrABJJJIAzrrtirqrqjqixzsrAjrqjiqaqqysttAJqjikikrizrHtBJJAzArzrIsRCITKSS099ijrAJS____Qg99habbCAYrDzh00E0b4x400000000h4g000000014h000000004h400000000p214FBMgI109EYeCBcLJmmbCLqfFfu8TJvTgo9HTdjYJEYR0

		//some sort of basic fanfare:
		tones( triangle, midi2hz( Gs(4)), 0.5f * 60.0f / bpm, "1 . . . 2 . . . 3 . . . 4 . . . ");
		tones( triangle, midi2hz( F(4) ), 0.5f * 60.0f / bpm, "1 . . . 2 . . . 3 . #===4 . . . ");
		tones( triangle, midi2hz( E(4) ), 0.5f * 60.0f / bpm, "1 . . . 2 . . . 3 . . . 4 . . . ");
		tones( triangle, midi2hz( Ds(4)), 0.5f * 60.0f / bpm, "1 . . . 2 . . . 3 . o===4 . . . ");
		tones( triangle, midi2hz( Cs(4)), 0.5f * 60.0f / bpm, "1 . . . 2 . . . 3 . . . 4 . . . ");
		tones( triangle, midi2hz( As(3)), 0.5f * 60.0f / bpm, "1 . #=. #=. . . o=. o===4 . . . ");
		tones( triangle, midi2hz( Gs(3)), 0.5f * 60.0f / bpm, "1 . . . 2 # . . 3 . . . 4 . . . ");
		tones( triangle, midi2hz( F(3) ), 0.5f * 60.0f / bpm, "1 .#. #=2 .#. . #=. o===4 . . . ");
		tones( triangle, midi2hz( E(3) ), 0.5f * 60.0f / bpm, "1 # . . 2 . #=. 3 . . . 4 . . . ");
		tones( triangle, midi2hz( Ds(3)), 0.5f * 60.0f / bpm, "# . . . 2 . . . o=. . . 4 . . . ");
		tones( triangle, midi2hz( Cs(3)), 0.5f * 60.0f / bpm, "1 . . . 2 . . .#3 . . . 4 . . . ");
		tones( triangle, midi2hz( As(2)), 0.5f * 60.0f / bpm, "1 . . . 2 . . . o=. . . 4 . . . ");

		//sub-bass:
		tones( sine, midi2hz( Gs(3)), 0.5f * 60.0f / bpm, "1 . . . 2 . . . 3 . . . 4 . . . ");
		tones( sine, midi2hz( F(3) ), 0.5f * 60.0f / bpm, "1 . . . 2 . . . 3 . . . 4 . . . ");
		tones( sine, midi2hz( E(3) ), 0.5f * 60.0f / bpm, "1 . . . 2 . . . 3 . . . 4 . . . ");
		tones( sine, midi2hz( Ds(3)), 0.5f * 60.0f / bpm, "1 . . . 2 . . . 3 . . . 4 . . . ");
		tones( sine, midi2hz( Cs(3)), 0.5f * 60.0f / bpm, "1 . . . 2 . . . 3 . . . o===. . ");
		tones( sine, midi2hz( As(2)), 0.5f * 60.0f / bpm, "1 . o===2 . . . 3 . . . 4 . . . ");
		tones( sine, midi2hz( Gs(2)), 0.5f * 60.0f / bpm, "o===. . 2 . o===3 .o.o.o4 . . . ");
		tones( sine, midi2hz( F(2) ), 0.5f * 60.0f / bpm, "1 . . . 2 . . . 3 o o o 4 . . . ");
		tones( sine, midi2hz( E(2) ), 0.5f * 60.0f / bpm, "1 . . . o===. . 3 . . . 4 . . . ");
		tones( sine, midi2hz( Ds(2)), 0.5f * 60.0f / bpm, "1 . . . 2 . . . 3 . . . 4 . . . ");
		tones( sine, midi2hz( Cs(2)), 0.5f * 60.0f / bpm, "1 . . . 2 . . . 3 . . . 4 . . . ");
		tones( sine, midi2hz( As(1)), 0.5f * 60.0f / bpm, "1 . . . 2 . . . 3 . . . 4 . . . ");

		//flourish:
		tones( square, midi2hz( F(4) ), 0.5f * 60.0f / bpm, "1 . . . 2 . . . 3 . o . 4 . . . ");
		tones( square, midi2hz( Ds(4)), 0.5f * 60.0f / bpm, "1 . . . 2 . . . 3 o . . 4 . . . ");
		tones( square, midi2hz( As(3)), 0.5f * 60.0f / bpm, "1 . . . 2 . . . 3o. . . 4 . . . ");
		tones( square, midi2hz( Gs(3)), 0.5f * 60.0f / bpm, "1 . . . 2 . . . o . . . 4 . . . ");

		{ //gently re-center output (remove dc):
			float smoothed = 0.0f;
			for (float &s : data) {
				smoothed += 1e-4f * (s - smoothed);
				s -= smoothed;
			}
		}

		{ //run a basic 'digital reverb' over stuff:
			//use a delay line with a few different taps and feedbacks:
			std::array< float, 2 * 48000 > delay;
			for (auto &s : delay) s = 0.0f; //clear delay line
			uint32_t head = 0;
			auto tap = [&](float offset) -> float {
				return delay[(head + int32_t(delay.size()) - int32_t(std::floor(offset * 48000.0f))) % delay.size()];
			};
			float smoothed = 0.0f;
			for (float &s : data) {
				float wet = 1.0f * s + (
					+ 6.0f * tap(0.43f)
					+ 5.0f * tap(0.21f)
					+ 3.0f * tap(0.13f)
					+ 1.0f * tap(0.09f)
					) / 16.0f;
				smoothed += 0.95f * (wet - smoothed); //smooth off high frequencies before writing reverb buffer
				delay[head] = smoothed;
				head = (head + 1) % delay.size();
				s = 0.1f * tap(0.25f) + 0.9f * s;
			}
		}

		{ //do a gentle low-pass filter on output:
			float smoothed = 0.0f;
			for (float &s : data) {
				smoothed += 0.3f * (s - smoothed);
				s = smoothed;
			}
			smoothed = 0.0f;
			for (float &s : data) {
				smoothed += 0.3f * (s - smoothed);
				s = smoothed;
			}
		}

		{ //fade out the last bit, just in case:
			constexpr uint32_t fade = 48000/2;
			for (uint32_t i = 0; i < fade; ++i) {
				float amt = (i + 0.5f) / float(fade);
				data[data.size()-1-i] *= amt;
			}
		}

		/*{ //DEBUG:
			std::ofstream dump("music-dump.f32", std::ios::binary);
			dump.write(reinterpret_cast< const char * >(data.data()), data.size() * 4);
		}*/

		static std::unique_ptr< Sound::Sample > music_sample; //making static so it lives past lifetime of IntroMode

		if (!music_sample) { //<-- when playing repeatedly for testing, want to avoid deallocating sample
			music_sample = std::make_unique< Sound::Sample >(data);
		}

		music = Sound::play(*music_sample);
	}

	// ------ shader ------
	//(based on ColorProgram.cpp)
	color_program = gl_compile_program(
		"#version 330\n"
		"uniform mat4 OBJECT_TO_CLIP;\n"
		"in vec4 Position;\n"
		"in vec4 Color;\n"
		"out vec4 color;\n"
		"void main() {\n"
		"	gl_Position = OBJECT_TO_CLIP * Position;\n"
		"	color = Color;\n"
		"}\n"
	,
		//fragment shader:
		"#version 330\n"
		"in vec4 color;\n"
		"out vec4 fragColor;\n"
		"void main() {\n"
		"	fragColor = color;\n"
		"}\n"
	);

	//look up the locations of vertex attributes:
	GLuint Position_vec4 = glGetAttribLocation(color_program, "Position");
	GLuint Color_vec4 = glGetAttribLocation(color_program, "Color");

	//look up the locations of uniforms:
	OBJECT_TO_CLIP_mat4 = glGetUniformLocation(color_program, "OBJECT_TO_CLIP");

	// ------ vertex buffer + vertex array object ------
	// (based on DrawLines.cpp)

	glGenVertexArrays(1, &vertex_buffer_for_color_program);
	glBindVertexArray(vertex_buffer_for_color_program);

	glGenBuffers(1, &vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);

	glVertexAttribPointer(
		Position_vec4, //attribute
		2, //size
		GL_FLOAT, //type
		GL_FALSE, //normalized
		sizeof(GP22IntroMode::Vertex), //stride
		(GLbyte *)0 + offsetof(GP22IntroMode::Vertex, Position) //offset
	);
	glEnableVertexAttribArray(Position_vec4);

	glVertexAttribPointer(
		Color_vec4, //attribute
		4, //size
		GL_UNSIGNED_BYTE, //type
		GL_TRUE, //normalized
		sizeof(GP22IntroMode::Vertex), //stride
		(GLbyte *)0 + offsetof(GP22IntroMode::Vertex, Color) //offset
	);
	glEnableVertexAttribArray(Color_vec4);

	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glBindVertexArray(0);

	GL_ERRORS();

	// ------ gp22 logo ------
	{
		//quick-n-hack-y svg path interpreter:
		std::vector< std::vector< glm::vec2 > > paths;
		[[maybe_unused]] auto svg_L = [&paths](std::vector< float > const &coords) {
			assert(!coords.empty() && coords.size() % 2 == 0);
			assert(!paths.empty());
			auto &path = paths.back();
			for (uint32_t i = 0; i + 1 < coords.size(); i += 2) {
				path.emplace_back(coords[i], coords[i+1]);
			}
		};
		[[maybe_unused]] auto svg_M = [&paths](std::vector< float > const &coords) {
			assert(!coords.empty() && coords.size() % 2 == 0);
			paths.emplace_back();
			auto &path = paths.back();
			path.emplace_back(coords[0], coords[1]);
			for (uint32_t i = 2; i + 1 < coords.size(); i += 2) {
				path.emplace_back(coords[i], coords[i+1]);
			}
		};
		[[maybe_unused]] auto svg_m = [&svg_M](std::vector< float > coords) {
			for (uint32_t i = 2; i < coords.size(); ++i) {
				coords[i] += coords[i-2];
			}
			svg_M(coords);
		};
		[[maybe_unused]] auto svg_C = [&paths](std::vector< float > const &coords) {
			assert(coords.size() >= 6 && (coords.size() % 6 == 0));
			assert(!paths.empty() && !paths.back().empty());
			auto &path = paths.back();
			for (uint32_t i = 0; i + 5 < coords.size(); i += 6) {
				glm::vec2 p0 = path.back();
				glm::vec2 p1 = glm::vec2(coords[i+0], coords[i+1]);
				glm::vec2 p2 = glm::vec2(coords[i+2], coords[i+3]);
				glm::vec2 p3 = glm::vec2(coords[i+4], coords[i+5]);

				for (uint32_t ti = 0; ti < 30; ++ti) {
					float t = (ti + 1.0f) / 30.0f;
					glm::vec2 p01 = glm::mix(p0, p1, t);
					glm::vec2 p12 = glm::mix(p1, p2, t);
					glm::vec2 p23 = glm::mix(p2, p3, t);
					glm::vec2 p012 = glm::mix(p01, p12, t);
					glm::vec2 p123 = glm::mix(p12, p23, t);
					glm::vec2 p = glm::mix(p012, p123, t);
					path.emplace_back(p);
				}
			}
		};
		[[maybe_unused]] auto svg_c = [&svg_C,&paths](std::vector< float > coords) {
			assert(coords.size() >= 6 && (coords.size() % 6 == 0));
			assert(!paths.empty() && !paths.back().empty());
			coords[0] += paths.back().back().x;
			coords[1] += paths.back().back().y;
			coords[2] += paths.back().back().x;
			coords[3] += paths.back().back().y;
			coords[4] += paths.back().back().x;
			coords[5] += paths.back().back().y;
			for (uint32_t i = 6; i + 5 < coords.size(); i += 6) {
				coords[i+0] += coords[i-2];
				coords[i+1] += coords[i-1];
				coords[i+2] += coords[i-2];
				coords[i+3] += coords[i-1];
				coords[i+4] += coords[i-2];
				coords[i+5] += coords[i-1];
			}
			svg_C(coords);
		};

		//the letters:

		//m 233.7,392.20001 39.3,-14.5
		svg_m({233.7f,392.20001f, 39.3f,-14.5f});
		//M 283.1,297
		svg_M({283.1f,297.0f});
		// c 0,-18.6 -29.7,-20.8 -29.7,7 0,8.1 -0.2,161.7 -0.8,171.7 -1.3,22.6 -29.1,24.9 -29.1,0.7
		svg_c({0.0f,-18.6f, -29.7f,-20.8f, -29.7f,7.0f, 0.0f,8.1f, -0.2f,161.7f, -0.8f,171.7f, -1.3f,22.6f, -29.1f,24.9f, -29.1f,0.7f});
		//M 30.1,323.8
		svg_M({30.1f,323.8f});
		// c 11.7,10 22.6,18.8 35.9,23.3 8,2.7 18,4 25.8,3.3 19.4,-1.7 23.8,-23.5 -1.5,-24.5 -46,-1.8 -56.3,25.8 -57.9,75.4 -2,63.5 79.6,65.9 81,20.2
		svg_c({11.7f,10.0f, 22.6f,18.8f, 35.9f,23.3f, 8.0f,2.7f, 18.0f,4.0f, 25.8f,3.3f, 19.4f,-1.7f, 23.8f,-23.5f, -1.5f,-24.5f, -46.0f,-1.8f, -56.3f,25.8f, -57.9f,75.4f, -2.0f,63.5f, 79.6f,65.9f, 81.0f,20.2f});
		// L 88.2,421
		svg_L({88.2f,421.0f});
		//M 190,455
		svg_M({190.0f,455.0f});
		// c -21.5,-0.2 -38.3,-10.6 -39.6,-33.8 -2.8,-47.1 1.2,-93 27.1,-95.2 45.6,-3.9 39.6,51.8 3.7,51.8 -32.5,0 -37.6,-27.2 -46.2,-48.7
		svg_c({-21.5f,-0.2f, -38.3f,-10.6f, -39.6f,-33.8f, -2.8f,-47.1f, 1.2f,-93.0f, 27.1f,-95.2f, 45.6f,-3.9f, 39.6f,51.8f, 3.7f,51.8f, -32.5f,0.0f, -37.6f,-27.2f, -46.2f,-48.7f});

		//the background blob:
		//M 500.4,125.7
		svg_M({500.4f,125.7f});
		// C 500.93475,240.3073 385.46997,227.48934 251.4,227.6 113.90005,227.7135 2.3283937,240.39998 2.4,125.7 2.4696771,14.090185 113.90006,23.673852 251.4,23.8
		svg_C({500.93475f,240.3073f, 385.46997f,227.48934f, 251.4f,227.6f, 113.90005f,227.7135f, 2.3283937f,240.39998f, 2.4f,125.7f, 2.4696771f,14.090185f, 113.90006f,23.673852f, 251.4f,23.8f});
		// c 141.6062,0.129915 248.51369,-2.324812 249,101.9
		svg_c({141.6062f,0.129915f, 248.51369f,-2.324812f, 249.0f,101.9f});
		// z
		paths.back().emplace_back(paths.back()[0]); //probably not needed

		blob = std::move(paths.back());
		paths.pop_back();
		for (auto &v : blob) {
			v = 0.02f * (v + glm::vec2(-254.0f, -123.0f));
			v.y = -v.y;
		}

		assert(!paths.empty());
		{ //offset to center middle of first path:
			assert(!paths.empty() && paths[0].size() == 2);
			glm::vec2 offset = -0.5f * (paths[0][0] + paths[0][1]);
			float scale = 0.02f;
			for (auto &path : paths) {
				for (auto &v : path) {
					v = scale * (v + offset);
					v.y = -v.y;
				}
			}
		}

		std::reverse(paths[2].begin(), paths[2].end());

		{ //last two letters get rotated copies:
			auto rc = [&](uint32_t si) {
				paths.emplace_back(paths.at(si));
				for (auto &v : paths.back()) {
					v = -v;
				}
			};
			rc(2);
			rc(3);
		}

		assert(paths.size() == 6);
		letters.resize(4);
		letters[0].base = std::move(paths[2]);
		letters[1].base = std::move(paths[3]);
		letters[2].base = std::move(paths[4]);
		letters[3].base = std::move(paths[5]);

		std::array< float, 4 > strikes{
			(8 * 2) * 0.5f * 60.0f / bpm,
			(8 * 2+1) * 0.5f * 60.0f / bpm,
			(8 * 2+2) * 0.5f * 60.0f / bpm,
			(8 * 2+4) * 0.5f * 60.0f / bpm
		};
		std::shuffle(strikes.begin(), strikes.end(), mt);
		for (uint32_t i = 0; i < 4; ++i) {
			letters[i].strike = strikes[i];
		}

		middle.resize(2);
		middle[0] = std::move(paths[0]);
		middle[1] = std::move(paths[1]);
	}

}

GP22IntroMode::~GP22IntroMode() {
}

bool GP22IntroMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	if (evt.type == SDL_KEYDOWN) {
		//on any key press, skip the rest of the intro:
		music->set_volume(0.0f, 1.0f / 10.0f);
		Mode::set_current(next_mode);
		return true;
	}
	return false;
}

void GP22IntroMode::update(float elapsed) {
	time += elapsed;
	if (time > 10.0f) {
		music->set_volume(0.0f, 1.0f / 10.0f);
		Mode::set_current(next_mode);
		//Mode::set_current(std::make_shared< GP22IntroMode >(next_mode)); //<--- loop forever for testing
		return;
	}

}

void GP22IntroMode::draw(glm::uvec2 const &drawable_size) {
	//requested visible bounds:
	glm::vec2 scene_min = glm::vec2(-5.0f, -3.0f);
	glm::vec2 scene_max = glm::vec2( 5.0f,  3.0f);

	{ //actually, zoom those bounds out a bit:
		glm::vec2 center = 0.5f * (scene_min + scene_max);
		glm::vec2 radius = 0.5f * (scene_max - scene_min);
		scene_min = center - 2.0f * radius;
		scene_max = center + 2.0f * radius;
	}

	//computed visible bounds:
	glm::vec2 screen_min, screen_max;

	//compute matrix for vertex shader:
	glm::mat4 object_to_clip;

	{ //make sure scene_min - scene_max fits in drawable_size:
		float aspect = drawable_size.x / float(drawable_size.y);
		float scale = glm::min(
			2.0f * aspect / (scene_max.x - scene_min.x),
			2.0f / (scene_max.y - scene_min.y)
		);
		object_to_clip = glm::mat4(
			scale / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, scale, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			scale / aspect * -0.5f * (scene_max.x + scene_min.x), scale * -0.5f * (scene_max.y + scene_min.y), 0.0f, 1.0f
		);

		screen_min = 0.5f * (scene_max + scene_min) - glm::vec2(aspect, 1.0f) / scale;
		screen_max = 0.5f * (scene_max + scene_min) + glm::vec2(aspect, 1.0f) / scale;
	}
	
	//compute triangles to draw:
	std::vector< Vertex > attribs;
	
	size_t bg_start = attribs.size();
	//--- striped background ---
	{
		float angle = 52.0f / 180.0f * float(M_PI);
		glm::vec2 dir = glm::vec2(std::cos(angle), std::sin(angle));
		glm::vec2 perp = glm::vec2(-dir.y, dir.x);

		float ofs = time * -0.7f;

		float w = 0.7f;

		auto [perp_min, perp_max] = std::minmax({
			glm::dot(perp, glm::vec2(screen_min.x, screen_min.y)),
			glm::dot(perp, glm::vec2(screen_max.x, screen_min.y)),
			glm::dot(perp, glm::vec2(screen_min.x, screen_max.y)),
			glm::dot(perp, glm::vec2(screen_max.x, screen_max.y))
		});

		auto [dir_min, dir_max] = std::minmax({
			glm::dot(dir, glm::vec2(screen_min.x, screen_min.y)),
			glm::dot(dir, glm::vec2(screen_max.x, screen_min.y)),
			glm::dot(dir, glm::vec2(screen_min.x, screen_max.y)),
			glm::dot(dir, glm::vec2(screen_max.x, screen_max.y))
		});

		for (int32_t s = int32_t(std::floor(perp_min / w - ofs)); s <= int32_t(std::floor(perp_max / w - ofs)); ++s) {
			if (!attribs.empty()) attribs.emplace_back(attribs.back());
			attribs.emplace_back(perp * ((ofs + s) * w) + dir * dir_min, Bg1);
			if (attribs.size() != 1) attribs.emplace_back(attribs.back());
			attribs.emplace_back(perp * ((ofs + s + 0.5f) * w) + dir * dir_min, Bg1);
			attribs.emplace_back(perp * ((ofs + s) * w) + dir * dir_max, Bg1);
			attribs.emplace_back(perp * ((ofs + s + 0.5f) * w) + dir * dir_max, Bg1);
		}
	}
	size_t bg_end = attribs.size();

	float fade_in = std::min(1.0f, time / 2.0f);
	uint8_t fade8 = uint8_t(fade_in * 255.0f);

	size_t mg_start = attribs.size();
	//--- background blob ---
	for (uint32_t i = 0; i < blob.size()/2; ++i) {
		if (i == 0 && !attribs.empty()) attribs.emplace_back(attribs.back());
		attribs.emplace_back(blob[i], glm::u8vec4(glm::u8vec3(Fg), fade8));
		if (i == 0 && attribs.size() != 1) attribs.emplace_back(attribs.back());
		attribs.emplace_back(blob[blob.size()-i-1], glm::u8vec4(glm::u8vec3(Fg), fade8));
	}

	{ //middle clef:
		for (auto const &m : middle) {
			std::vector< float > len;
			len.reserve(m.size());
			double total = 0.0;
			len.emplace_back(float(total));
			for (uint32_t i = 1; i < m.size(); ++i) {
				total += glm::length(m[i] - m[i-1]);
				len.emplace_back(float(total));
			}
			stroke(&attribs, m, len, 0.16f, glm::u8vec4(glm::u8vec3(FgFill2), fade8), {}, glm::u8vec4(glm::u8vec3(FgFill1), fade8));
		}
	}
	size_t mg_end = attribs.size();

	size_t fg_start = attribs.size();
	//--- letters ---
	for (auto const &letter : letters) {
		letter.compute(&attribs, time);
	}
	size_t fg_end = attribs.size();

	float spin = 0.0f;
	for (auto const &letter : letters) {
		if (time > letter.strike) {
			float amt = std::min((time - letter.strike) / 2.0f, 1.0f);
			amt = 1.0f - (std::pow(0.1f, amt) - 0.1f) / 0.9f;
			spin += amt;
		}
	}
	spin *= glm::radians(540.0f) / letters.size();

	glm::mat4 fg_to_clip = object_to_clip * glm::rotate(spin, glm::vec3(0.0f, 0.0f, 1.0f));
	glm::mat4 mg_to_clip = fg_to_clip * glm::scale(glm::vec3(1.0f + 0.2f * std::pow(1.0f - fade_in,2.0f))); //maybe?

	//----- actually draw ----
	// (Based on DrawLines.cpp)

	//upload attribs:
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer); //set vertex_buffer as current
	glBufferData(GL_ARRAY_BUFFER, attribs.size() * sizeof(attribs[0]), attribs.data(), GL_STREAM_DRAW); //upload attribs array
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	//Clear background:
	glClearColor(Bg2.r / 255.0f, Bg2.g / 255.0f, Bg2.b / 255.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	//No depth test, please, but yes let's use alpha blending:
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glUseProgram(color_program);
	glUniformMatrix4fv(OBJECT_TO_CLIP_mat4, 1, GL_FALSE, glm::value_ptr(object_to_clip));

	glBindVertexArray(vertex_buffer_for_color_program);
	glDrawArrays(GL_TRIANGLE_STRIP, GLint(bg_start), GLsizei(bg_end - bg_start));

	glUniformMatrix4fv(OBJECT_TO_CLIP_mat4, 1, GL_FALSE, glm::value_ptr(mg_to_clip));
	glDrawArrays(GL_TRIANGLE_STRIP, GLint(mg_start), GLsizei(mg_end - mg_start));

	glUniformMatrix4fv(OBJECT_TO_CLIP_mat4, 1, GL_FALSE, glm::value_ptr(fg_to_clip));
	glDrawArrays(GL_TRIANGLE_STRIP, GLint(fg_start), GLsizei(fg_end - fg_start));
	glBindVertexArray(0);

	glUseProgram(0);

	GL_ERRORS();
}