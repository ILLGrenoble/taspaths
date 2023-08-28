/**
 * triple axis angle calculation
 * @author Tobias Weber <tweber@ill.fr>
 * @date jul-2021
 * @license GPLv3, see 'LICENSE' file
 *
 * ----------------------------------------------------------------------------
 * TAS-Paths (part of the Takin software suite)
 * Copyright (C) 2021  Tobias WEBER (Institut Laue-Langevin (ILL),
 *                     Grenoble, France).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * ----------------------------------------------------------------------------
 */

#ifndef __TASCALC_H__
#define __TASCALC_H__

#include <optional>

#include "tlibs2/libs/maths.h"
#include "types.h"


/**
 * tas angles
 */
struct TasAngles
{
	bool mono_ok = false;
	bool ana_ok = false;
	bool sample_ok = false;

	t_real monoXtalAngle = 0;
	t_real anaXtalAngle = 0;
	t_real sampleXtalAngle = 0;
	t_real sampleScatteringAngle = 0;

	t_real distance = 0;
};


/**
 * helper class for tas calculations
 */
class TasCalculator
{
public:
	TasCalculator() = default;
	~TasCalculator() = default;

	void SetMonochromatorD(t_real d);
	void SetAnalyserD(t_real d);
	void SetSampleAngleOffset(t_real offs);

	std::pair<t_real, bool> GetKfix() const;
	void SetKfix(bool fixed_kf = true);
	void SetKfix(t_real k_fix, bool fixed_kf);
	void SetKfix(t_real k_fix);

	void SetKf(t_real kf) { SetKfix(kf, true); }
	void SetKi(t_real ki) { SetKfix(ki, false); }

	void SetScatteringSenses(bool monoccw, bool sampleccw, bool anaccw);
	const t_real* GetScatteringSenses() const;

	void SetSampleLatticeConstants(t_real a, t_real b, t_real c);
	void SetSampleLatticeAngles(t_real alpha, t_real beta, t_real gamma, bool deg = false);
	void SetSampleScatteringPlane(
		t_real vec1_x, t_real vec1_y, t_real vec1_z,
		t_real vec2_x, t_real vec2_y, t_real vec2_z);
	const t_vec& GetSampleScatteringPlane(std::size_t vecidx) const;

	// get crystal matrices
	const t_mat& GetB() const;
	const t_mat& GetUB() const;

	// update crystal matrices
	void UpdateB();
	void UpdateScatteringPlane();
	void UpdateUB();

	// calculate instrument coordinates in xtal system
	std::tuple<std::optional<t_vec>, t_real>
	GetQE(t_real monoXtalAngle, t_real anaXtalAngle,
		t_real sampleXtalAngle, t_real sampleScAngle) const;

	// calculate instrument angles
	TasAngles GetAngles(t_real h, t_real k, t_real l, t_real ki, t_real kf) const;
	TasAngles GetAngles(t_real h, t_real k, t_real l, t_real E) const;


private:
	// crystal lattice constants
	t_vec m_lattice = tl2::create<t_vec>({ 5, 5, 5 });

	// crystal lattice angles
	t_vec m_angles = tl2::create<t_vec>({
		tl2::pi<t_real> * 0.5,
		tl2::pi<t_real> * 0.5,
		tl2::pi<t_real> * 0.5
	});

	// crystal matrices
	t_mat m_B = tl2::B_matrix<t_mat>(
		m_lattice[0], m_lattice[1], m_lattice[0],
		m_angles[0], m_angles[1], m_angles[2]);
	t_mat m_UB = tl2::unit<t_mat>(3);

	// scattering plane
	t_vec m_plane_rlu[3] = {
		tl2::create<t_vec>({ 1, 0, 0 }),
		tl2::create<t_vec>({ 0, 1, 0 }),
		tl2::create<t_vec>({ 0, 0, 1 }),
	};

	// mono and ana d-spacings
	t_real m_dspacings[2] = { 3.355, 3.355 };

	// scattering senses
	t_real m_sensesCCW[3] = { 1., -1., 1. };

	// sample angle offset
	t_real m_a3Offs = tl2::pi<t_real>;

	// fixed ki or kf value
	bool m_fixed_kf = true;
	t_real m_kfix = 1.4;
};


#endif
