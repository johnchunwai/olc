#include "olcNoiseMaker.h"
#include <iostream>
#include <algorithm>
#include <chrono>

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
        instrument* instru = nullptr;   // the instrument for this note

        note(int id, ftype on, ftype off, bool active, instrument *instru)
            : id{ id }, on{ on }, off{ off }, active{ active }, instru{ instru }
        {
        }
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
    ftype osc(ftype time, ftype hertz, osc_t type, ftype lfo_hertz = 0.0, ftype lfo_amp = 0.0, ftype custom = 50.0)
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
            for (auto n = 1.0; n < custom; ++n) {
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
        ftype max_life;
        wstring name;

        instrument(ftype v, ftype attk_time, ftype decay_time, ftype release_time, ftype attk_amp, ftype sustain_amp, ftype max_life, wstring_view name)
            : vol{ v }
            , env{ attk_time, decay_time, release_time, attk_amp, sustain_amp }
            , max_life{max_life}
            , name(name)
        {
        }
        virtual ~instrument() = default;

        instrument(const instrument&) = delete;
        instrument& operator=(const instrument&) = delete;

        virtual sound_ret_t sound(ftype time, note n) const = 0;
    };

    struct bell : public instrument
    {
        bell()
            : instrument(1.0, 0.01, 1.0, 1.0, 1.0, 0.0, 0.0, L"Bell")
        {
        }

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
            : instrument(1.0, 0.01, 0.5, 1.0, 1.0, 0.8, 0.0, L"8-bit Bell")
        {
        }

        virtual sound_ret_t sound(ftype time, note n) const override
        {
            bool note_finished = false;
            auto amp = env.amplitude(time, n.on, n.off);
            if (amp < g_epsilon) {
                note_finished = true;
            }
            auto sound = 1.0 * osc(time - n.on, scale(n.id), osc_t::square, 5.0, 0.001)
                + 0.5 * osc(time - n.on, scale(n.id + 12), osc_t::sine)
                + 0.25 * osc(time - n.on, scale(n.id + 24), osc_t::sine);
            return { amp * sound * vol, note_finished };
        }
    };

    struct harmonica : public instrument
    {
        harmonica()
            : instrument(0.3, 0.0, 1.0, 0.1, 1.0, 0.95, 0.0, L"Harmonica")
        {
        }

        virtual sound_ret_t sound(ftype time, note n) const override
        {
            bool note_finished = false;
            auto amp = env.amplitude(time, n.on, n.off);
            if (amp < g_epsilon) {
                note_finished = true;
            }
            auto sound = 1.0 * osc(n.on - time, scale(n.id - 12), osc_t::saw_analog, 5.0, 0.001, 100)
                + 1.0 * osc(time - n.on, scale(n.id), osc_t::square, 5.0, 0.001)
                + 0.5 * osc(time - n.on, scale(n.id + 12), osc_t::square)
                + 0.05 * osc(time - n.on, 0.0, osc_t::noise);
            return  { amp * sound * vol, note_finished };
        }
    };

    struct drumkick : public instrument
    {
        drumkick()
            : instrument(1.0, 0.01, 0.15, 0.0, 1.0, 0.0, 1.5, L"Drum Kick")
        {
        }

        virtual sound_ret_t sound(ftype time, note n) const override
        {
            bool note_finished = false;
            auto amp = env.amplitude(time, n.on, n.off);
            if (max_life > 0.0 && time - n.on >= max_life) {
                note_finished = true;
            }
            auto sound = 0.99 * osc(time - n.on, scale(n.id - 36), osc_t::sine, 1.0, 1.0)
                + 0.01 * osc(time - n.on, 0.0, osc_t::noise);
            return  { amp * sound * vol, note_finished };
        }
    };

    struct drumsnare : public instrument
    {
        drumsnare()
            : instrument(1.0, 0.0, 0.2, 0.0, 1.0, 0.0, 1.0, L"Drum Snare")
        {
        }

        virtual sound_ret_t sound(ftype time, note n) const override
        {
            bool note_finished = false;
            auto amp = env.amplitude(time, n.on, n.off);
            if (max_life > 0.0 && time - n.on >= max_life) {
                note_finished = true;
            }
            auto sound = 0.5 * osc(time - n.on, scale(n.id - 24), osc_t::sine, 0.5, 1.0)
                + 0.5 * osc(time - n.on, 0.0, osc_t::noise);
            return  { amp * sound * vol, note_finished };
        }
    };

    struct drumhihat : public instrument
    {
        drumhihat()
            : instrument(1.0, 0.01, 0.05, 0.0, 1.0, 0.0, 1.0, L"Drum HiHat")
        {
        }

        virtual sound_ret_t sound(ftype time, note n) const override
        {
            bool note_finished = false;
            auto amp = env.amplitude(time, n.on, n.off);
            if (max_life > 0.0 && time - n.on >= max_life) {
                note_finished = true;
            }
            auto sound = 0.1 * osc(time - n.on, scale(n.id - 12), osc_t::square, 1.5, 1.0)
                + 0.9 * osc(time - n.on, 0.0, osc_t::noise);
            return  { amp * sound * vol, note_finished };
        }
    };

    struct sequencer
    {
        struct channel
        {
            instrument* instru = nullptr;
            wstring beat_pattern = L""s;

            channel(instrument* instru, wstring_view beat_pattern)
                : instru(instru), beat_pattern(beat_pattern)
            {
            }
        };

        int beats;
        int subbeats;
        ftype tempo;
        ftype beat_time;
        ftype accum_time;
        int curr_beat;
        int total_beats;

        vector<channel> channel_vec;
        vector<note> note_vec;

        sequencer(ftype tempo = 120.0f, int beats = 4, int subbeats = 4)
            : beats{ beats }
            , subbeats{ subbeats }
            , tempo{ tempo }
            , beat_time { 60.0 / tempo / (ftype) subbeats}
            , accum_time { 0.0 }
            , curr_beat{ 0 }
            , total_beats{ subbeats * beats }
            , channel_vec{}
            , note_vec{}
        {
        }

        void add_instrument(instrument* instru, wstring_view beat_pattern)
        {
            channel_vec.emplace_back(instru, beat_pattern);
        }

        int update(ftype elapsed)
        {
            note_vec.clear();

            accum_time += elapsed;
            while (accum_time >= beat_time) {
                accum_time -= beat_time;

                for (auto ch : channel_vec) {
                    if (ch.beat_pattern[curr_beat] == L'X') {
                        //int id = 0;                     // position in scale, id 0 is the note with g_octave_base_freq (A3)
                        //ftype on = 0.0;                 // time when note is on
                        //ftype off = 0.0;                // time when note is off
                        //bool active = false;            // when false, this note will be removed from the vector to make sound
                        //instrument* instru = nullptr;   // the instrument for this note
                        note_vec.emplace_back(64, 0.0, 0.0, true, ch.instru);
                    }
                }

                curr_beat = (curr_beat + 1) % total_beats;
            }
            return note_vec.size();
        }
    };
}


static auto init_instrument_vec()
{
    vector<unique_ptr<synth::instrument>> instru_vec;
    instru_vec.push_back(make_unique<synth::bell>());
    instru_vec.push_back(make_unique<synth::bell8>());
    instru_vec.push_back(make_unique<synth::harmonica>());
    return instru_vec;
}

vector<synth::note> g_note_list;
mutex g_notes_mutex;
vector<unique_ptr<synth::instrument>> g_instruments{ init_instrument_vec() };
atomic<synth::instrument*> g_active_instrument = g_instruments[0].get();
synth::drumkick instru_kick;
synth::drumsnare instru_snare;
synth::drumhihat instru_hihat;
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
        auto [sound, note_finished] = n.instru->sound(time, n);
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

    synth::sequencer seq(90.0);
    seq.add_instrument(&instru_kick,  L"X...X...X..X.X..");
    seq.add_instrument(&instru_snare, L"..X...X...X...X.");
    seq.add_instrument(&instru_hihat, L"X.X.X.X.X.X.X.XX");

    auto clock_prev_time = chrono::high_resolution_clock::now();
    auto clock_curr_time = chrono::high_resolution_clock::now();
    ftype elapsed = 0.0;
    ftype wall_time = 0.0;

    constexpr string_view keys{ "AWSDRFTGHUJIKOL\xba\xdb\xde\xdd" };
    constexpr string_view instrument_keys{ "123" };
    //constexpr string_view osc_keys{ "123456" };
    //constexpr string_view env_keys{ "90" };
    int curr_key = -1;
    int curr_instrument_key = 0;
    //int curr_osc_key = -1;
    //int curr_env_key = -1;
    while (1) {
        //bool key_pressed = false;

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

        for (int k = 0; k < instrument_keys.size(); k++) {
            if (GetAsyncKeyState((uint8_t)instrument_keys[k]) & 0x8000) {
                if (curr_instrument_key != k) {
                    curr_instrument_key = k;
                    g_active_instrument = g_instruments[k].get();
                }
            }
        }

        clock_curr_time = chrono::high_resolution_clock::now();
        elapsed = chrono::duration<ftype>(clock_curr_time - clock_prev_time).count();
        clock_prev_time = clock_curr_time;
        wall_time += elapsed;
        auto sound_time = sound.GetTime();

        // update sequencer
        int new_notes = seq.update(elapsed);
        if (new_notes > 0) {
            scoped_lock lock(g_notes_mutex);
            for (auto n : seq.note_vec) {
                n.on = sound_time;
                g_note_list.push_back(n);
            }
        }

        for (int k = 0; k < keys.size(); k++) {
            auto key_state = GetAsyncKeyState((uint8_t)keys[k]);

            // handle key press and release
            scoped_lock lock(g_notes_mutex);
            auto found_note = find_if(g_note_list.begin(), g_note_list.end(), [k](const synth::note& n) { return n.id == k; });
            if (key_state & 0x8000) {
                if (found_note != g_note_list.end()) {
                    // key on and note exist
                    // use >= instead of > to avoid the case when off==on and it does nothing
                    if (found_note->off >= found_note->on) {
                        // key was off but on again
                        found_note->on = sound_time;
                    }
                    else {
                        // key was on and held, do nothing
                    }
                }
                else {
                    // key on and note not exist, create
                    g_note_list.emplace_back(k, sound_time, 0.0, true, g_active_instrument);
                }
            }
            else {
                if (found_note != g_note_list.end()) {
                    // key off and note exist
                    // use >= instead of > to avoid the case when off==on and it does nothing
                    if (found_note->on >= found_note->off) {
                        // key was on, set to off
                        found_note->off = sound_time;
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
