#include "olcNoiseMaker.h"
#include <iostream>

using namespace std;


// oscillator type
enum class osc_t
{
    sin = 1, square, triangle, saw_additive, saw_mod, pseudo_rand
};

struct envelope_adsr
{
    double attk_time = 0.2;
    double decay_time = 0.2;
    double release_time = 0.5;

    double sustain_amp = 0.8;
    double attk_amp = 1.0;

    double trigger_on_time = 0.0;
    double trigger_off_time = 0.0;
    bool note_on = false;

    double get_amp(double time)
    {
        auto amp = 0.0;

        if (note_on) {
            // ADS
            auto life_time = time - trigger_on_time;
            if (life_time <= attk_time) {
                // attacking
                amp = attk_amp * life_time / attk_time;
            }
            else if (life_time <= (attk_time + decay_time)) {
                // decaying
                amp = attk_amp - (attk_amp - sustain_amp) * (life_time - attk_time) / decay_time;
            }
            else {
                // sustaining
                amp = sustain_amp;
            }
        }
        else {
            // R
            // use release time to calculate amp
            auto dead_time = time - trigger_off_time;
            amp = sustain_amp + (-sustain_amp) * dead_time / release_time;
        }

        if (amp < 0.0001) {
            amp = 0.0;
        }

        return amp;
    }

    void set_note_on(double time)
    {
        note_on = true;
        trigger_on_time = time;
    }


    void set_note_off(double time)
    {
        note_on = false;
        trigger_off_time = time;
    }
};


atomic<double> g_freq = 0.0;
constexpr double g_octave_base_freq = 220.0; // A3 (A below middle C)
const double g_12th_root_of_2 = pow(2.0, 1.0 / 12.0);
atomic<osc_t> g_osc_type{ osc_t::saw_additive};
envelope_adsr envelope{};
atomic<bool> g_use_envelope = true;

// w -> omega - angular velocity from hertz
double w(double hertz)
{
    return hertz * 2 * PI;
}

// osc -> oscillate
double osc(double hertz, double time, osc_t type)
{
    switch (type) {
    case osc_t::sin:
        return sin(w(hertz) * time);
    case osc_t::square:
        return sin(w(hertz) * time) > 0.0 ? 1.0 : -1.0;
    case osc_t::triangle:
        return abs(fmod(time * hertz * 4.0, 4.0) - 2.0) - 1.0;
        // alternatively
        //return asin(sin(w(hertz) * time)) * 2.0 / PI;
    case osc_t::saw_additive:
    {
        // adding different frequencies of sine waves creates the saw wave
        // supposed to be warmer?
        auto output = 0.0;
        for (auto n = 1.0; n < 20.0; ++n) {
            output += sin(n * w(hertz) * time) / n;
        }
        return output * 2.0 / PI;
    }
    case osc_t::saw_mod:
        return fmod(time * hertz, 1.0) * 2.0 - 1.0;
    case osc_t::pseudo_rand:
        // scale down it to make it less harsh
        return (((double)rand() / (double)RAND_MAX) * 2.0 - 1.0) * 0.5;
    default:
        return 0.0;
    }
}

double sound_func(double time)
{
    if (g_freq == 0.0) {
        return 0.0;
    }
    //double output = sin(g_freq * 2 * PI * time);
    // for cord
    //double output = sin(w(g_freq) * time);// +sin(w(g_freq) * pow(g_12th_root_of_2, 2) * time) + sin(w(g_freq) * pow(g_12th_root_of_2, 4) * time);
    double output = 0.0;
    if (g_use_envelope) {
         output = envelope.get_amp(time) * osc(g_freq, time, g_osc_type);
         // mixing with saw wave at an octave lower
         output = envelope.get_amp(time) * (osc(g_freq, time, g_osc_type) + osc(g_freq * 0.5, time, osc_t::saw_additive));
    }
    else {
        output = osc(g_freq, time, g_osc_type);
    }
    // for square wave
    //return (output > 0.0) ? 0.2 : -0.2;

    // for sine wave
    return 0.4 * output;
}


int main()
{
    // get sound hardwares
    vector<wstring> devices = olcNoiseMaker<int16_t>::Enumerate();
    for (auto d : devices) wcout << "Found sound device: " << d << endl;

    // Display a keyboard
    wcout << endl <<
        "|   |   |   |   |   | |   |   |   |   | |   | |   |   |   |   | |   |" << endl <<
        "|   | W |   |   | R | | T |   |   | U | | I | | O |   |   | [ | | ] |" << endl <<
        "|   |___|   |   |___| |___|   |   |___| |___| |___|   |   |___| |___|" << endl <<
        "|     |     |     |     |     |     |     |     |     |     |     | " << endl <<
        "|  A  |  S  |  D  |  F  |  G  |  H  |  J  |  K  |  L  |  ;  |  '  | " << endl <<
        "|_____|_____|_____|_____|_____|_____|_____|_____|_____|_____|_____|_" << endl << endl;

    // create sound machine
    // 16 bit sample
    olcNoiseMaker<int16_t> sound(devices[0], 44100, 1, 8, 512);
    sound.SetUserFunction(sound_func);

    constexpr string_view keys{ "AWSDRFTGHUJIKOL\xba\xdb\xde\xdd" };
    constexpr string_view osc_keys{ "123456" };
    constexpr string_view env_keys{ "90" };
    int curr_key = -1;
    int curr_osc_key = -1;
    int curr_env_key = -1;
    while (1) {
        bool key_pressed = false;

        for (int k = 0; k < env_keys.size(); k++) {
            if (GetAsyncKeyState((uint8_t)env_keys[k]) & 0x8000) {
                if (curr_env_key != k) {
                    curr_env_key = k;
                    g_use_envelope = k;
                    if (!g_use_envelope) {
                        g_freq = 0.0;
                    }
                }
            }
        }

        for (int k = 0; k < osc_keys.size(); k++) {
            if (GetAsyncKeyState((uint8_t)osc_keys[k]) & 0x8000) {
                if (curr_osc_key != k) {
                    curr_osc_key = k;
                    g_osc_type = osc_t{ k + 1 };
                }
            }
        }

        for (int k = 0; k < keys.size(); k++) {
            if (GetAsyncKeyState((uint8_t)keys[k]) & 0x8000) {
                if (curr_key != k) {
                    curr_key = k;
                    g_freq = g_octave_base_freq * pow(g_12th_root_of_2, k);
                    wcout << L"Note on: " << sound.GetTime() << "s " << g_freq << "Hz" << endl;
                    envelope.set_note_on(sound.GetTime());
                }
                key_pressed = true;
            }
        }

        if (!key_pressed) {
            if (curr_key != -1) {
                curr_key = -1;
                if (!g_use_envelope) {
                    g_freq = 0.0;
                }
                envelope.set_note_off(sound.GetTime());
            }
        }
    }
}