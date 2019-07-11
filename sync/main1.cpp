#include "olcNoiseMaker.h"
#include <iostream>
#include <algorithm>

using namespace std;

using ftype = double;

namespace synth
{
    constexpr ftype g_epsilon = 0.00001;
    constexpr ftype g_octave_base_freq = 220.0; // A3 (A below middle C)

    // w -> omega - angular velocity from hertz
    ftype w(ftype hertz)
    {
        return hertz * 2 * PI;
    }

    struct instrument;

    // a basic note
    struct note
    {
        int id = 0;                     // position in scale, id 0 is the note with g_octave_base_freq (A3)
        ftype on = 0.0;                 // time when note is on
        ftype off = 0.0;                // time when note is off
        bool active = false;            // when false, this note will be removed from the vector to make sound
        instrument* channel = nullptr;  // each channel is handled by an instrument
    };

    // scale to freq conversion
    ftype scale(const int note_id)
    {
        // 1.0594630943592952645618252949463 == pow(2.0, 1.0 / 12.0)
        return g_octave_base_freq * pow(1.0594630943592952645618252949463, note_id);
    }

    // oscillator type
    enum class osc_t : int
    {
        sine, square, triangle, saw_analog, saw_digital, noise
    };

    // osc -> oscillate
    // lfo -> low frequency oscillator for modulating the frequency of the wave
    ftype osc(ftype time, ftype hertz, osc_t type, ftype lfo_hertz = 0.0, ftype lfo_amp = 0.0)
    {
        auto freq = w(hertz) * time + lfo_amp * hertz * sin(w(lfo_hertz) * time);
        switch (type) {
        case osc_t::sine:
            return sin(freq);
        case osc_t::square:
            return sin(freq) > 0.0 ? 1.0 : -1.0;
        case osc_t::triangle:
            //return abs(fmod(time * hertz * 4.0, 4.0) - 2.0) - 1.0;
            // alternatively
            return asin(sin(freq)) * 2.0 / PI;
        case osc_t::saw_analog:
        {
            // adding different frequencies of sine waves creates the saw wave
            // supposed to be warmer?
            auto output = 0.0;
            for (auto n = 1.0; n < 20.0; ++n) {
                output += sin(n * freq) / n;
            }
            return output * 2.0 / PI;
        }
        case osc_t::saw_digital:
            return fmod(freq / (2.0 * PI), 1.0) * 2.0 - 1.0;
            //        return fmod(time * hertz, 1.0) * 2.0 - 1.0;
        case osc_t::noise:
            // scale down it to make it less harsh
            return ((ftype)rand() / (ftype)RAND_MAX) * 2.0 - 1.0;// * 0.5;
        default:
            return 0.0;
        }
    }

    //
    // Envelopes
    //

    struct envelope
    {
        virtual ftype amplitude(const ftype time, const ftype time_on, const ftype time_off) const = 0;
    };

    struct envelope_adsr : public envelope
    {
        ftype attk_time;
        ftype decay_time;
        ftype release_time;
        ftype attk_amp;
        ftype sustain_amp;

        envelope_adsr(ftype attk_t, ftype decay_t, ftype release_t, ftype attk_a, ftype sustain_a)
            : attk_time{ attk_t }
            , decay_time{decay_t}
            , release_time{release_t}
            , attk_amp{ attk_a }
            , sustain_amp{ sustain_a }
        {
        }

        virtual ftype amplitude(const ftype time, const ftype time_on, const ftype time_off) const override
        {
            auto amp = 0.0;
            bool note_on = time_on > time_off;
            auto life_time = note_on ? time - time_on : time_off - time_on;
            auto on_amp = 0.0;
            if (life_time <= attk_time) {
                // attacking (A)
                on_amp = attk_amp * life_time / attk_time;
            }
            else if (life_time <= (attk_time + decay_time)) {
                // decaying (D)
                on_amp = attk_amp - (attk_amp - sustain_amp) * (life_time - attk_time) / decay_time;
            }
            else {
                // sustaining (S)
                on_amp = sustain_amp;
            }

            if (note_on) {
                amp = on_amp;
            }
            else {
                // releasing (R)
                // release from on_amp which depends on which phase it is in when the note is released
                auto dead_time = time - time_off;
                amp = on_amp + (-on_amp) * dead_time / release_time;
            }

            // handle precision and don't allow negative
            if (amp < g_epsilon) {
                amp = 0.0;
            }

            return amp;
        }
    };

    struct instrument
    {
        struct sound_ret_t { ftype amp; bool note_finished; };

        ftype vol;
        envelope_adsr env;

        instrument(ftype v, ftype attk_time, ftype decay_time, ftype release_time, ftype attk_amp, ftype sustain_amp)
            : vol{ v }
            , env{ attk_time, decay_time, release_time, attk_amp, sustain_amp }
        {
        }
        virtual ~instrument() {}

        instrument(const instrument&) = delete;
        instrument& operator=(const instrument&) = delete;

        virtual sound_ret_t sound(ftype time, note n) const = 0;
    };

    struct bell : public instrument
    {
        bell()
            : instrument(1.0, 0.01, 1.0, 1.0, 1.0, 0.0)
        {
        }

        virtual ~bell() {}

        virtual sound_ret_t sound(ftype time, note n) const override
        {
            bool note_finished = false;
            auto amp = env.amplitude(time, n.on, n.off);
            if (amp < g_epsilon) {
                note_finished = true;
            }
            auto sound = 1.0 * osc(time, scale(n.id + 12), osc_t::sine, 5.0, 0.001)
                + 0.5 * osc(time, scale(n.id + 24), osc_t::sine)
                + 0.25 * osc(time, scale(n.id + 36), osc_t::sine);
            return { amp * sound * vol, note_finished };
        }
    };

    struct bell8 : public instrument
    {
        bell8()
            : instrument(1.0, 0.01, 1.0, 1.0, 1.0, 0.8)
        {
        }

        virtual ~bell8() {}

        virtual sound_ret_t sound(ftype time, note n) const override
        {
            bool note_finished = false;
            auto amp = env.amplitude(time, n.on, n.off);
            if (amp < g_epsilon) {
                note_finished = true;
            }
            auto sound = 1.0 * osc(time, scale(n.id + 12), osc_t::sine, 5.0, 0.001)
                + 0.5 * osc(time, scale(n.id + 24), osc_t::sine)
                + 0.25 * osc(time, scale(n.id + 36), osc_t::sine);
            return { amp * sound * vol, note_finished };
        }
    };

    struct harmonica : public instrument
    {
        harmonica()
            : instrument(0.5, 0.0, 0.0, 0.0, 1.0, 0.8)
        {
        }

        virtual ~harmonica() {}

        virtual sound_ret_t sound(ftype time, note n) const override
        {
            bool note_finished = false;
            auto amp = env.amplitude(time, n.on, n.off);
            if (amp < g_epsilon) {
                note_finished = true;
            }
            auto sound = 1.0 * osc(time, scale(n.id), osc_t::square, 5.0, 0.001)
                + 0.5 * osc(time, scale(n.id + 12), osc_t::square)
                + 0.05 * osc(time, scale(n.id + 24), osc_t::noise);
            return  { amp * sound * vol, note_finished };
        }
    };

    //struct test1 : public instrument
    //{
    //    test1()
    //    {
    //        env = { 0.0,  0.0, 0.0, 0.8, 1.0 };
    //    }

    //    ftype sound(ftype time, ftype freq) override
    //    {
    //        auto output = env.amplitude(time) * (
    //            osc(freq, time, osc_t::saw_analog, 2.0, 0.1)
    //            );
    //        return output;
    //    }
    //};

    //struct test2 : public instrument
    //{
    //    test2()
    //    {
    //        env = { 0.1, 0.01, 0.2, 0.8, 1.0 };
    //    }

    //    ftype sound(ftype time, ftype freq) override
    //    {
    //        auto output = env.amplitude(time) * (
    //            osc(freq, time, osc_t::saw_digital, 2.0, 0.1)
    //            );
    //        return output;
    //    }
    //};
}


static auto init_channel_vec()
{
    vector<unique_ptr<synth::instrument>> channels;
    channels.emplace_back(make_unique<synth::bell>());
    channels.emplace_back(make_unique<synth::bell8>());
    channels.emplace_back(make_unique<synth::harmonica>());
    return channels;
}

vector<synth::note> g_note_list;
mutex g_notes_mutex;
vector<unique_ptr<synth::instrument>> g_channels{ init_channel_vec() };
atomic<synth::instrument*> g_active_channel = g_channels[0].get();
//shared_ptr<instrument> g_instrument = make_shared<harmonica>();

template<typename V, typename Func>
void safe_remove_if(V& v, Func func)
{
    for (auto it = v.begin(); it != v.end(); ) {
        if (func(*it)) {
            it = v.erase(it);
        }
        else {
            ++it;
        }
    }
}

ftype sound_func(int channel, ftype time)
{
    ftype mixed_sound = 0.0;
    scoped_lock lock(g_notes_mutex);
    for (auto& n : g_note_list) {
        auto [sound, note_finished] = n.channel->sound(time, n);
        mixed_sound += sound;
        if (note_finished && n.off > n.on) {
            n.active = false;
        }
    }

    safe_remove_if(g_note_list, [](const synth::note& n) { return !n.active; });

    return mixed_sound * 0.2;
}


int main()
{
    // get sound hardwares
    vector<wstring> devices = olcNoiseMaker<int16_t>::Enumerate();
    for (const auto& d : devices) wcout << "Found sound device: " << d << endl;
    wcout << "Using device: " << devices[0] << endl;

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
    constexpr string_view channel_keys{ "123" };
    //constexpr string_view osc_keys{ "123456" };
    //constexpr string_view env_keys{ "90" };
    int curr_key = -1;
    int curr_channel_key = 0;
    //int curr_osc_key = -1;
    //int curr_env_key = -1;
    while (1) {
        bool key_pressed = false;

        //for (int k = 0; k < env_keys.size(); k++) {
        //    if (GetAsyncKeyState((uint8_t)env_keys[k]) & 0x8000) {
        //        if (curr_env_key != k) {
        //            curr_env_key = k;
        //            g_use_envelope = k;
        //            if (!g_use_envelope) {
        //                g_freq = 0.0;
        //            }
        //        }
        //    }
        //}

        //for (int k = 0; k < osc_keys.size(); k++) {
        //    if (GetAsyncKeyState((uint8_t)osc_keys[k]) & 0x8000) {
        //        if (curr_osc_key != k) {
        //            curr_osc_key = k;
        //            g_osc_type = osc_t{ k + 1 };
        //        }
        //    }
        //}

        for (int k = 0; k < channel_keys.size(); k++) {
            if (GetAsyncKeyState((uint8_t)channel_keys[k]) & 0x8000) {
                if (curr_channel_key != k) {
                    curr_channel_key = k;
                    g_active_channel = g_channels[k].get();
                }
            }
        }

        for (int k = 0; k < keys.size(); k++) {
            auto key_state = GetAsyncKeyState((uint8_t)keys[k]);
            auto now = sound.GetTime();

            // handle key press and release
            scoped_lock lock(g_notes_mutex);
            auto found_note = find_if(g_note_list.begin(), g_note_list.end(), [k](const synth::note& n) { return n.id == k; });
            if (key_state & 0x8000) {
                if (found_note != g_note_list.end()) {
                    // key on and note exist
                    // use >= instead of > to avoid the case when off==on and it does nothing
                    if (found_note->off >= found_note->on) {
                        // key was off but on again
                        found_note->on = now;
                    }
                    else {
                        // key was on and held, do nothing
                    }
                }
                else {
                    // key on and note not exist, create
                    g_note_list.push_back({ k, now, 0.0, true, g_active_channel });
                }
            }
            else {
                if (found_note != g_note_list.end()) {
                    // key off and note exist
                    // use >= instead of > to avoid the case when off==on and it does nothing
                    if (found_note->on >= found_note->off) {
                        // key was on, set to off
                        found_note->off = now;
                    }
                    else {
                        // key was off and stay off, do nothing
                    }
                }
                else {
                    // key off and note not exist, do nothing
                }
            }
        }
        //wcout << "Notes: " << g_note_list.size() << endl;
        //for (int k = 0; k < keys.size(); k++) {
        //    if (GetAsyncKeyState((uint8_t)keys[k]) & 0x8000) {
        //        if (curr_key != k) {
        //            curr_key = k;
        //            wcout << L"Note on: " << sound.GetTime() << "s, Note ID: " << k << endl;
        //            g_instrument->env.set_note_on(sound.GetTime());
        //        }
        //        key_pressed = true;
        //    }
        //}

        //if (!key_pressed) {
        //    if (curr_key != -1) {
        //        curr_key = -1;
        //        //if (!g_use_envelope) {
        //        //    g_freq = 0.0;
        //        //}
        //        g_instrument->env.set_note_off(sound.GetTime());
        //    }
        //}
    }
}
