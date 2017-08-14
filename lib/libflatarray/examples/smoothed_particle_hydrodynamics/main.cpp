/**
 * Copyright 2016-2017 Andreas Schäfer
 * Copyright 2017 Google
 *
 * Distributed under the Boost Software License, Version 1.0. (See accompanying
 * file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
 */

#ifdef _MSC_BUILD
#pragma warning( push )
#pragma warning( disable : 4514 )
#endif

#include <cmath>

#ifdef _MSC_BUILD
#pragma warning( pop )
#endif

#include <iostream>
#include <silo.h>
#include <stdlib.h>
#include <vector>
#include <libflatarray/flat_array.hpp>

#include "kernels.h"
#include "kernels.hpp"

int box_indicator(float x, float y)
{
    return (x < 0.25) && (y < 1.0);
}

void place_particles(
    int count,
    float *pos_x,
    float *pos_y,
    float *v_x,
    float *v_y,
    float hh)
{
    int index = 0;

    for (float x = 0; x < 1; x += hh) {
        for (float y = 0; y < 1; y += hh) {
            if (box_indicator(x,y)) {
                pos_x[index] = x;
                pos_y[index] = y;
                v_x[index] = 0;
                v_y[index] = 0;
                ++index;
            }
        }
    }
}

int count_particles(float hh)
{
    int ret = 0;

    for (float x = 0; x < 1; x += hh) {
        for (float y = 0; y < 1; y += hh) {
            ret += box_indicator(x,y);
        }
    }

    return ret;
}

void normalize_mass(float *mass, int n, float *rho, float rho0)
{
    float rho_squared_sum = 0;
    float rho_sum = 0;

    for (int i = 0; i < n; ++i) {
        rho_squared_sum += rho[i] * rho[i];
        rho_sum += rho[i];
    }

    *mass = *mass * rho0 * rho_sum / rho_squared_sum;
}

void dump_time_step(int cycle, int n, float* pos_x, float* pos_y)
{
    DBfile *dbfile = NULL;
    char filename[100];
    sprintf(filename, "output%04d.silo", cycle);
    dbfile = DBCreate(filename, DB_CLOBBER, DB_LOCAL,
                      "simulation time step", DB_HDF5);

    float *coords[] = {(float*)pos_x, (float*)pos_y};
    DBPutPointmesh(dbfile, "pointmesh", 2, coords, n,
                   DB_FLOAT, NULL);

    DBClose(dbfile);
}

class Particle
{
public:
    class API
    {
    public:
        LIBFLATARRAY_CUSTOM_SIZES(
            (32)(64)(128)(256)(512)(1024)(2048)(4096)(8192)(16384)(32768)(65536),
            (1),
            (1))
    };

    float rho;
    float pos_x;
    float pos_y;
    float v_x;
    float v_y;
    float a_x;
    float a_y;
};

LIBFLATARRAY_REGISTER_SOA(Particle, ((float)(rho))((float)(pos_x))((float)(pos_y))((float)(v_x))((float)(v_y))((float)(a_x))((float)(a_y)));


int main_c(int argc, char** argv)
{
    // time step length:
    float dt = 1e-4;
    // pitch: (size of particles)
    float h = 2e-2;
    // target density:
    float rho0 = 1000;
    // bulk modulus:
    float k = 1e3;
    // viscosity:
    float mu = 0.1;
    // gravitational acceleration:
    float g = 9.8;

    float hh = h / 1.3;
    int count = count_particles(hh);

    std::vector<float> rho(count);
    std::vector<float> pos_x(count);
    std::vector<float> pos_y(count);
    std::vector<float> v_x(count);
    std::vector<float> v_y(count);
    std::vector<float> a_x(count);
    std::vector<float> a_y(count);

    place_particles(count, pos_x.data(), pos_y.data(), v_x.data(), v_y.data(), hh);
    float mass = 1;
    compute_density(count, rho.data(), pos_x.data(), pos_y.data(), h, mass);
    normalize_mass(&mass, count, rho.data(), rho0);

    int num_steps = 20000;
    int io_period = 20000;

    for (int t = 0; t < num_steps; ++t) {
        if ((t % io_period) == 0) {
            dump_time_step(t, count, pos_x.data(), pos_y.data());
        }

        compute_density(
            count,
            rho.data(),
            pos_x.data(),
            pos_y.data(),
            h,
            mass);

        compute_accel(
            count,
            rho.data(),
            pos_x.data(),
            pos_y.data(),
            v_x.data(),
            v_y.data(),
            a_x.data(),
            a_y.data(),
            mass,
            g,
            h,
            k,
            rho0,
            mu);

        leapfrog(
            count,
            pos_x.data(),
            pos_y.data(),
            v_x.data(),
            v_y.data(),
            a_x.data(),
            a_y.data(),
            dt);

        reflect_bc(
            count,
            pos_x.data(),
            pos_y.data(),
            v_x.data(),
            v_y.data());
    }

    dump_time_step(num_steps, count, pos_x.data(), pos_y.data());

    return 0;
}

class Simulate
{
public:
    Simulate(
        float dt,
        float h,
        float rho0,
        float k,
        float mu,
        float g,
        float hh,
        int count) :
        dt(dt),
        h(h),
        rho0(rho0),
        k(k),
        mu(mu),
        g(g),
        hh(hh),
        count(count)
    {}

    template<typename SOA_ACCESSOR>
    void operator()(SOA_ACCESSOR& particles)
    {
        place_particles(count, &particles.pos_x(), &particles.pos_y(), &particles.v_x(), &particles.v_y(), hh);
        float mass = 1;
        compute_density(count, &particles.rho(), &particles.pos_x(), &particles.pos_y(), h, mass);
        normalize_mass(&mass, count, &particles.rho(), rho0);

        int num_steps = 20000;
        int io_period = 15;

        for (int t = 0; t < num_steps; ++t) {
            if ((t % io_period) == 0) {
                dump_time_step(t, count, &particles.pos_x(), &particles.pos_y());
            }

            compute_density_lfa(
                count,
                particles,
                h,
                mass);

            compute_accel_lfa(
                count,
                particles,
                mass,
                g,
                h,
                k,
                rho0,
                mu);

            leapfrog(
                count,
                &particles.pos_x(),
                &particles.pos_y(),
                &particles.v_x(),
                &particles.v_y(),
                &particles.a_x(),
                &particles.a_y(),
                dt);

            reflect_bc(
                count,
                &particles.pos_x(),
                &particles.pos_y(),
                &particles.v_x(),
                &particles.v_y());
        }

        dump_time_step(num_steps, count, &particles.pos_x(), &particles.pos_y());
    }

private:
    float dt;
    float h;
    float rho0;
    float k;
    float mu;
    float g;
    float hh;
    int count;
};

int main(int argc, char** argv)
{
    // time step length:
    float dt = 1e-4;
    // pitch: (size of particles)
    float h = 2e-2;
    // target density:
    float rho0 = 1000;
    // bulk modulus:
    float k = 1e3;
    // viscosity:
    float mu = 0.1;
    // gravitational acceleration:
    float g = 9.8;

    float hh = h / 1.3;
    int count = count_particles(hh);

    LibFlatArray::soa_grid<Particle> particles(count, 1, 1);

    Simulate sim_functor(dt, h, rho0, k, mu, g, hh, count);
    particles.callback(sim_functor);

    return 0;
}
