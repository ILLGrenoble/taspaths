/**
 * tlibs2 -- magnetic dynamics
 * @author Tobias Weber <tweber@ill.fr>
 * @date 2022 - 2024
 * @license GPLv3, see 'LICENSE' file
 *
 * References:
 *   - (Toth 2015) S. Toth and B. Lake, J. Phys.: Condens. Matter 27 166002 (2015):
 *                 https://doi.org/10.1088/0953-8984/27/16/166002
 *                 https://arxiv.org/abs/1402.6069
 *   - (Heinsdorf 2021) N. Heinsdorf, manual example calculation for a simple
 *                      ferromagnetic case, personal communications, 2021/2022.
 *
 * @desc This file implements the formalism given by (Toth 2015).
 * @desc For further references, see the 'LITERATURE' file.
 *
 * ----------------------------------------------------------------------------
 * tlibs
 * Copyright (C) 2017-2024  Tobias WEBER (Institut Laue-Langevin (ILL),
 *                          Grenoble, France).
 * Copyright (C) 2015-2017  Tobias WEBER (Technische Universitaet Muenchen
 *                          (TUM), Garching, Germany).
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

#ifndef __TLIBS2_MAGDYN_H__
#define __TLIBS2_MAGDYN_H__

#include <algorithm>
#include <numeric>
#include <vector>
#include <array>
#include <tuple>
#include <unordered_set>
#include <unordered_map>
#include <string>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <cstdint>

#include <boost/container_hash/hash.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#ifndef USE_LAPACK
	#define USE_LAPACK 1
#endif
#include "maths.h"
#include "units.h"
#include "phys.h"
#include "algos.h"
#include "expr.h"

// enables debug output
//#define __TLIBS2_MAGDYN_DEBUG_OUTPUT__
//#define __TLIBS2_MAGDYN_DEBUG_PY_OUTPUT__



namespace tl2_mag {

// ----------------------------------------------------------------------------
// helper functions
// ----------------------------------------------------------------------------

/**
 * rotate spin vector for incommensurate structures, i.e. helices
 */
template<class t_mat, class t_vec, class t_real = typename t_mat::value_type>
#ifndef SWIG  // TODO: remove this as soon as swig understands concepts
requires tl2::is_mat<t_mat> && tl2::is_vec<t_vec>
#endif
void rotate_spin_incommensurate(t_vec& spin_vec,
	const t_vec& sc_vec, const t_vec& ordering, const t_vec& rotaxis,
	t_real eps = std::numeric_limits<t_real>::epsilon())
{
	const t_real sc_angle = t_real(2) * tl2::pi<t_real> *
		tl2::inner<t_vec>(ordering, sc_vec);

	if(!tl2::equals_0<t_real>(sc_angle, t_real(eps)))
	{
		t_mat sc_rot = tl2::rotation<t_mat, t_vec>(rotaxis, sc_angle);
		spin_vec = sc_rot * spin_vec;
	}
}



/**
 * polarisation matrix
 * @see https://doi.org/10.1088/1361-6463/aa7573
 */
template<class t_mat, class t_cplx = typename t_mat::value_type>
#ifndef SWIG  // TODO: remove this as soon as swig understands concepts
requires tl2::is_mat<t_mat>
#endif
t_mat get_polarisation(int channel = 0, bool in_chiral_basis = true)
{
	if(in_chiral_basis)
	{
		t_mat pol = tl2::zero<t_mat>(3);

		// just pick the selected component on the diagonal
		if(channel >=0 && channel < 3)
			pol(channel, channel) = t_cplx(1);

		return pol;
	}
	else
	{
		constexpr const t_cplx halfi = t_cplx(0, 0.5);
		constexpr const t_cplx half = t_cplx(0.5, 0);

		// TODO: check coordinate system
		switch(channel)
		{
			case 0: return tl2::create<t_mat>({
				{   half, +halfi,  0 },
				{ -halfi,   half,  0 },
				{      0,      0,  0 } });
			case 1: return tl2::create<t_mat>({
				{   half, -halfi,  0 },
				{ +halfi,   half,  0 },
				{      0,      0,  0 } });
			case 2: return tl2::create<t_mat>({
				{      0,      0,  0 },
				{      0,      0,  0 },
				{      0,      0,  1 } });
		}
	}

	return tl2::zero<t_mat>(3);
}
// ----------------------------------------------------------------------------



// ----------------------------------------------------------------------------
// input- and output struct templates
// ----------------------------------------------------------------------------

using t_strarr3 = std::array<std::string, 3>;
using t_strarr33 = std::array<std::array<std::string, 3>, 3>;



/**
 * magnetic sites
 */
template<class t_mat, class t_vec, class t_vec_real,
	class t_size, class t_real = typename t_vec_real::value_type>
struct t_MagneticSite
{
	// ------------------------------------------------------------------------
	// input properties
	std::string name{};          // identifier

	t_strarr3 pos{};             // magnetic site position

	t_strarr3 spin_dir{};        // spin direction
	t_strarr3 spin_ortho{};      // spin orthogonal vector

	std::string spin_mag{};      // spin magnitude
	t_mat g_e{};                 // electron g factor
	// ------------------------------------------------------------------------

	// ------------------------------------------------------------------------
	// calculated properties
	t_vec_real pos_calc{};       // magnetic site position

	t_vec spin_dir_calc{};       // spin vector
	t_vec spin_ortho_calc{};     // spin orthogonal vector
	t_vec spin_ortho_conj_calc{};

	t_real spin_mag_calc{};      // spin magnitude
	// ------------------------------------------------------------------------
};



/**
 * couplings between magnetic sites
 */
template<class t_mat, class t_vec, class t_vec_real,
	class t_size, class t_cplx = typename t_mat::value_type>
struct t_ExchangeTerm
{
	// ------------------------------------------------------------------------
	// input properties (parsable expressions)
	std::string name{};          // identifier

	std::string site1{};         // name of first magnetic site
	std::string site2{};         // name of second magnetic site
	t_strarr3 dist{};            // distance between unit cells

	std::string J{};             // Heisenberg interaction
	t_strarr3 dmi{};             // Dzyaloshinskij-Moriya interaction
	t_strarr33 Jgen{};           // general exchange interaction
	// ------------------------------------------------------------------------

	// ------------------------------------------------------------------------
	// calculated properties
	t_size site1_calc{};         // index of first magnetic site
	t_size site2_calc{};         // index of second magnetic site
	t_vec_real dist_calc{};      // distance between unit cells

	t_cplx J_calc{};             // Heisenberg interaction
	t_vec dmi_calc{};            // Dzyaloshinskij-Moriya interaction
	t_mat Jgen_calc{};           // general exchange interaction
	// ------------------------------------------------------------------------
};



/**
 * terms related to an external magnetic field
 */
template<class t_vec_real, class t_real>
struct t_ExternalField
{
	bool align_spins{};          // align spins along external field

	t_vec_real dir{};            // field direction
	t_real mag{};                // field magnitude
};



/**
 * eigenenergies and spin-spin correlation matrix
 */
template<class t_mat, class t_real>
struct t_EnergyAndWeight
{
	t_real E{};

	// full dynamical structure factor
	t_mat S{};
	t_real weight_full{};
	t_real weight_channel_full[3] = { 0., 0., 0. };

	// projected dynamical structure factor for neutron scattering
	t_mat S_perp{};
	t_real weight{};
	t_real weight_channel[3] = { 0., 0., 0. };
};



/**
 * variables for the expression parser
 */
template<class t_cplx>
struct t_Variable
{
	std::string name{};
	t_cplx value{};
};
// ----------------------------------------------------------------------------



/**
 * calculates magnon dynamics,
 * implementing the formalism given in (Toth 2015)
 */
template<
	class t_mat, class t_vec,
	class t_mat_real, class t_vec_real,
	class t_cplx = typename t_mat::value_type,
	class t_real = typename t_mat_real::value_type,
	class t_size = std::size_t>
#ifndef SWIG  // TODO: remove this as soon as swig understands concepts
requires tl2::is_mat<t_mat> && tl2::is_vec<t_vec> &&
	tl2::is_mat<t_mat_real> && tl2::is_vec<t_vec_real>
#endif
class MagDyn
{
public:
	// --------------------------------------------------------------------
	// structs and types
	// --------------------------------------------------------------------
	using MagneticSite = t_MagneticSite<t_mat, t_vec, t_vec_real, t_size, t_real>;
	using MagneticSites = std::vector<MagneticSite>;

	using ExchangeTerm = t_ExchangeTerm<t_mat, t_vec, t_vec_real, t_size, t_cplx>;
	using ExchangeTerms = std::vector<ExchangeTerm>;

	using ExternalField = t_ExternalField<t_vec_real, t_real>;
	using EnergyAndWeight = t_EnergyAndWeight<t_mat, t_real>;
	using Variable = t_Variable<t_cplx>;

	using t_indices = std::pair<t_size, t_size>;
	using t_Jmap = std::unordered_map<t_indices, t_mat, boost::hash<t_indices>>;
	// --------------------------------------------------------------------



public:
	MagDyn() = default;
	~MagDyn() = default;

	MagDyn(const MagDyn&) = default;
	MagDyn& operator=(const MagDyn&) = default;


	// --------------------------------------------------------------------
	// cleanup functions
	// --------------------------------------------------------------------
	/**
	 * clear all
	 */
	void Clear()
	{
		ClearVariables();
		ClearMagneticSites();
		ClearExchangeTerms();
		ClearExternalField();

		// clear temperature, -1: don't use
		m_temperature = -1.;

		// clear ordering wave vector
		m_ordering = tl2::zero<t_vec_real>(3);
		m_is_incommensurate = false;

		// reset rotation axis
		m_rotaxis = tl2::create<t_vec_real>({ 1., 0., 0. });
	}



	/**
	 * clear all parser variables
	 */
	void ClearVariables()
	{
		m_variables.clear();
	}



	/**
	 * clear all magnetic sites
	 */
	void ClearMagneticSites()
	{
		m_sites.clear();
	}



	/**
	 * clear all couplings
	 */
	void ClearExchangeTerms()
	{
		m_exchange_terms.clear();
	}



	/**
	 * clear the external field settings
	 */
	void ClearExternalField()
	{
		m_field.dir.clear();
		m_field.mag = 0.;
		m_field.align_spins = false;
	}
	// --------------------------------------------------------------------



	// --------------------------------------------------------------------
	// getter
	// --------------------------------------------------------------------
	const std::vector<Variable>& GetVariables() const { return m_variables; }
	const MagneticSites& GetMagneticSites() const { return m_sites; }
	MagneticSites& GetMagneticSites() { return m_sites; }
	t_size GetMagneticSitesCount() const { return m_sites.size(); }
	const ExchangeTerms& GetExchangeTerms() const { return m_exchange_terms; }
	ExchangeTerms& GetExchangeTerms() { return m_exchange_terms; }
	t_size GetExchangeTermsCount() const { return m_exchange_terms.size(); }

	const ExternalField& GetExternalField() const { return m_field; }
	const t_vec_real& GetRotationAxis() const { return m_rotaxis; }
	const t_vec_real& GetOrderingWavevector() const { return m_ordering; }

	t_real GetTemperature() const { return m_temperature; }
	t_real GetBoseCutoffEnergy() const { return m_bose_cutoff; }



	const MagneticSite& GetMagneticSite(t_size idx) const
	{
		if(!CheckMagneticSite(idx))
		{
			static MagneticSite null_site{};
			return null_site;
		}

		return m_sites[idx];
	}



	const ExchangeTerm& GetExchangeTerm(t_size idx) const
	{
		if(!CheckExchangeTerm(idx))
		{
			static ExchangeTerm null_term{};
			return null_term;
		}

		return m_exchange_terms[idx];
	}



	bool IsIncommensurate() const
	{
		return m_is_incommensurate || m_force_incommensurate;
	}



	/**
	 * get number of magnetic sites with the given name (to check if the name is unique)
	 */
	std::vector<const MagneticSite*> FindMagneticSites(const std::string& name) const
	{
		std::vector<const MagneticSite*> sites;

		for(const auto& site : GetMagneticSites())
		{
			if(site.name == name)
				sites.push_back(&site);
		}

		return sites;
	}



	/**
	 * get the index of a magnetic site from its name
	 */
	t_size GetMagneticSiteIndex(const std::string& name) const
	{
		// try to find the site index by name
		for(t_size idx = 0; idx < GetMagneticSitesCount(); ++idx)
		{
			if(GetMagneticSite(idx).name == name)
				return idx;
		}

		try
		{
			// alternatively try to parse the expression for the index
			tl2::ExprParser<t_size> parser;
			parser.SetInvalid0(false);
			parser.SetAutoregisterVariables(false);
			if(parser.parse(name))
			{
				if(t_size idx = parser.eval(); idx < GetMagneticSitesCount())
					return idx;
			}
		}
		catch(const std::exception& ex)
		{
			std::cerr << "Error: Invalid site name \"" << name << "\"."
				<< " Parser error: " << ex.what()
				<< std::endl;
		}

		// nothing found: return invalid index
		return GetMagneticSitesCount();
	}



	/**
	 * get the index of an exchange term from its name
	 */
	t_size GetExchangeTermIndex(const std::string& name) const
	{
		for(t_size idx = 0; idx < GetExchangeTermsCount(); ++idx)
		{
			if(GetExchangeTerm(idx).name == name)
				return idx;
		}

		try
		{
			// alternatively try to parse the expression for the index
			tl2::ExprParser<t_size> parser;
			parser.SetInvalid0(false);
			parser.SetAutoregisterVariables(false);
			if(parser.parse(name))
			{
				if(t_size idx = parser.eval(); idx < GetExchangeTermsCount())
					return idx;
			}
		}
		catch(const std::exception& ex)
		{
			std::cerr << "Error: Invalid coupling name \"" << name << "\"."
				<< " Parser error: " << ex.what()
				<< std::endl;
		}

		return GetExchangeTermsCount();  // return invalid index
	}
	// --------------------------------------------------------------------



	// --------------------------------------------------------------------
	// setter
	// --------------------------------------------------------------------
	void SetEpsilon(t_real eps) { m_eps = eps; }
	void SetPrecision(int prec) { m_prec = prec; }

	void SetTemperature(t_real T) { m_temperature = T; }
	void SetBoseCutoffEnergy(t_real E) { m_bose_cutoff = E; }
	void SetUniteDegenerateEnergies(bool b) { m_unite_degenerate_energies = b; }
	void SetForceIncommensurate(bool b) { m_force_incommensurate = b; }

	void SetPhaseSign(t_real sign) { m_phase_sign = sign; }
	void SetCholeskyMaxTries(t_size max_tries) { m_tries_chol = max_tries; }
	void SetCholeskyInc(t_real delta) { m_delta_chol = delta; }



	void SetExternalField(const ExternalField& field)
	{
		m_field = field;

		// normalise direction vector
		const t_real len = tl2::norm<t_vec_real>(m_field.dir);
		if(!tl2::equals_0<t_real>(len, m_eps))
			m_field.dir /= len;
	}



	void RotateExternalField(const t_vec_real& axis, t_real angle)
	{
		const t_mat_real rot = tl2::rotation<t_mat_real, t_vec_real>(
			axis, angle, false);
		m_field.dir = rot * m_field.dir;
	}



	void RotateExternalField(t_real x, t_real y, t_real z, t_real angle)
	{
		RotateExternalField(tl2::create<t_vec_real>({ x, y, z }), angle);
	}



	/**
	 * set the ordering wave vector (e.g., the helix pitch) for incommensurate structures
	 */
	void SetOrderingWavevector(const t_vec_real& ordering)
	{
		m_ordering = ordering;
		m_is_incommensurate = !tl2::equals_0<t_vec_real>(m_ordering, m_eps);
	}



	/**
	 * set the rotation axis for the ordering wave vector
	 */
	void SetRotationAxis(const t_vec_real& axis)
	{
		m_rotaxis = axis;

		// normalise
		const t_real len = tl2::norm<t_vec_real>(m_rotaxis);
		if(!tl2::equals_0<t_real>(len, m_eps))
			m_rotaxis /= len;
	}



	void SetCalcHamiltonian(bool H, bool Hp, bool Hm)
	{
		m_calc_H  = H;
		m_calc_Hp = Hp;
		m_calc_Hm = Hm;
	}



	void AddVariable(Variable&& var)
	{
		m_variables.emplace_back(std::forward<Variable&&>(var));
	}



	void SetVariable(Variable&& var)
	{
		// is a variable with the same name already registered?
		auto iter = std::find_if(m_variables.begin(), m_variables.end(),
			[&var](const auto& thevar)
		{
			return thevar.name == var.name;
		});

		if(iter == m_variables.end())
		{
			// add a new variable
			AddVariable(std::forward<Variable&&>(var));
		}
		else
		{
			// replace the value of an existing variable
			iter->value = var.value;
		}
	}



	void AddMagneticSite(MagneticSite&& site)
	{
		m_sites.emplace_back(std::forward<MagneticSite&&>(site));
	}



	void AddExchangeTerm(ExchangeTerm&& term)
	{
		m_exchange_terms.emplace_back(std::forward<ExchangeTerm&&>(term));
	}
	// --------------------------------------------------------------------



	// --------------------------------------------------------------------
	/**
	 * get an expression parser object with registered variables
	 */
	tl2::ExprParser<t_cplx> GetExprParser() const
	{
		tl2::ExprParser<t_cplx> parser;

		// register all variables
		parser.SetAutoregisterVariables(false);
		for(const Variable& var : m_variables)
			parser.register_var(var.name, var.value);

		return parser;
	}
	// --------------------------------------------------------------------



	/**
	 * get the needed supercell ranges from the exchange terms
	 */
	std::tuple<t_vec_real, t_vec_real> GetSupercellMinMax() const
	{
		t_vec_real min = tl2::zero<t_vec_real>(3);
		t_vec_real max = tl2::zero<t_vec_real>(3);

		for(const ExchangeTerm& term : GetExchangeTerms())
		{
			for(std::uint8_t i = 0; i < 3; ++i)
			{
				min[i] = std::min(min[i], term.dist_calc[i]);
				max[i] = std::max(max[i], term.dist_calc[i]);
			}
		}

		return std::make_tuple(min, max);
	}



	// --------------------------------------------------------------------
	// sanity checks
	// --------------------------------------------------------------------
	/**
	 * check if the site index is valid
	 */
	bool CheckMagneticSite(t_size idx, bool print_error = true) const
	{
		if(idx >= m_sites.size())
		{
			if(print_error)
			{
				std::cerr << "Error: Site index " << idx
					<< " is out of bounds."
					<< std::endl;
			}

			return false;
		}

		return true;
	}



	/**
	 * check if the term index is valid
	 */
	bool CheckExchangeTerm(t_size idx, bool print_error = true) const
	{
		if(idx >= m_exchange_terms.size())
		{
			if(print_error)
			{
				std::cerr << "Error: Coupling index " << idx
					<< " is out of bounds."
					<< std::endl;
			}

			return false;
		}

		return true;
	}
	// --------------------------------------------------------------------



	// --------------------------------------------------------------------
	// symmetrisation and generation functions
	// --------------------------------------------------------------------
	/**
	 * generate symmetric positions based on the given symops
	 */
	void SymmetriseMagneticSites(const std::vector<t_mat_real>& symops)
	{
		CalcExternalField();
		CalcMagneticSites();
		MagneticSites newsites;

		for(const MagneticSite& site : GetMagneticSites())
		{
			const auto positions = tl2::apply_ops_hom<t_vec_real, t_mat_real, t_real>(
				site.pos_calc, symops, m_eps);

			for(t_size idx = 0; idx < positions.size(); ++idx)
			{
				MagneticSite newsite = site;
				newsite.pos_calc = positions[idx];
				newsite.pos[0] = tl2::var_to_str(newsite.pos_calc[0]);
				newsite.pos[1] = tl2::var_to_str(newsite.pos_calc[1]);
				newsite.pos[2] = tl2::var_to_str(newsite.pos_calc[2]);
				newsite.name += "_" + tl2::var_to_str(idx + 1);

				newsites.emplace_back(std::move(newsite));
			}
		}

		m_sites = std::move(newsites);
		RemoveDuplicateMagneticSites();
		CalcMagneticSites();
	}



	/**
	 * generate symmetric exchange terms based on the given symops
	 */
	void SymmetriseExchangeTerms(const std::vector<t_mat_real>& symops)
	{
		CalcExternalField();
		CalcMagneticSites();
		CalcExchangeTerms();
		ExchangeTerms newterms;
		tl2::ExprParser<t_cplx> parser = GetExprParser();

		// create unit cell site vectors
		std::vector<t_vec_real> sites_uc;
		sites_uc.reserve(GetMagneticSitesCount());
		for(const MagneticSite& site : GetMagneticSites())
			sites_uc.push_back(tl2::create<t_vec_real>({
				site.pos_calc[0], site.pos_calc[1], site.pos_calc[2], 1. }));

		for(const ExchangeTerm& term : GetExchangeTerms())
		{
			// check if the site indices are valid
			if(!CheckMagneticSite(term.site1_calc) || !CheckMagneticSite(term.site2_calc))
				continue;

			// super cell distance vector
			t_vec_real dist_sc = tl2::create<t_vec_real>({
				term.dist_calc[0], term.dist_calc[1], term.dist_calc[2], 0. });

			// generate new (possibly supercell) sites with symop
			auto sites1_sc = tl2::apply_ops_hom<t_vec_real, t_mat_real, t_real>(
				sites_uc[term.site1_calc], symops, m_eps,
				false /*keep in uc*/, true /*ignore occupied*/, true);
			auto sites2_sc = tl2::apply_ops_hom<t_vec_real, t_mat_real, t_real>(
				sites_uc[term.site2_calc] + dist_sc, symops, m_eps,
				false /*keep in uc*/, true /*ignore occupied*/, true);

			// generate new dmi vectors
			t_vec_real dmi = tl2::zero<t_vec_real>(4);

			for(std::uint8_t dmi_idx = 0; dmi_idx < 3; ++dmi_idx)
			{
				if(term.dmi[dmi_idx] == "")
					continue;

				if(parser.parse(term.dmi[dmi_idx]))
				{
					dmi[dmi_idx] = parser.eval().real();
				}
				else
				{
					std::cerr << "Error parsing DMI component " << dmi_idx
						<< " of term \"" << term.name << "\"."
						<< std::endl;
				}
			}

			const auto newdmis = tl2::apply_ops_hom<t_vec_real, t_mat_real, t_real>(
				dmi, symops, m_eps, false, true);

			// generate new general J matrices
			t_real Jgen_arr[3][3]{};

			for(std::uint8_t J_idx1 = 0; J_idx1 < 3; ++J_idx1)
			{
				for(std::uint8_t J_idx2 = 0; J_idx2 < 3; ++J_idx2)
				{
					if(term.Jgen[J_idx1][J_idx2] == "")
						continue;

					if(parser.parse(term.Jgen[J_idx1][J_idx2]))
					{
						Jgen_arr[J_idx1][J_idx2] = parser.eval().real();
					}
					else
					{
						std::cerr << "Error parsing general J component ("
							<< J_idx1 << ", " << J_idx2
							<< ") of term \"" << term.name << "\"."
							<< std::endl;
					}
				}
			}

			t_mat_real Jgen = tl2::create<t_mat_real>({
				Jgen_arr[0][0], Jgen_arr[0][1], Jgen_arr[0][2], 0,
				Jgen_arr[1][0], Jgen_arr[1][1], Jgen_arr[1][2], 0,
				Jgen_arr[2][0], Jgen_arr[2][1], Jgen_arr[2][2], 0,
				0,              0,              0,              0 });

			const auto newJgens = tl2::apply_ops_hom<t_mat_real, t_real>(Jgen, symops);

			// iterate and insert generated couplings
			for(t_size op_idx = 0; op_idx < sites1_sc.size(); ++op_idx)
			{
				// get position of the site in the supercell
				const auto [sc1_ok, site1_sc_idx, sc1] = tl2::get_supercell(
					sites1_sc[op_idx], sites_uc, 3, m_eps);
				const auto [sc2_ok, site2_sc_idx, sc2] = tl2::get_supercell(
					sites2_sc[op_idx], sites_uc, 3, m_eps);

				if(!sc1_ok || !sc2_ok)
				{
					std::cerr << "Could not find supercell for position generated from symop "
						<< op_idx << "." << std::endl;
				}

				ExchangeTerm newterm = term;
				newterm.site1_calc = site1_sc_idx;
				newterm.site2_calc = site2_sc_idx;
				newterm.site1 = GetMagneticSite(newterm.site1_calc).name;
				newterm.site2 = GetMagneticSite(newterm.site2_calc).name;
				newterm.dist_calc = sc2 - sc1;
				newterm.dist[0] = tl2::var_to_str(newterm.dist_calc[0]);
				newterm.dist[1] = tl2::var_to_str(newterm.dist_calc[1]);
				newterm.dist[2] = tl2::var_to_str(newterm.dist_calc[2]);

				for(std::uint8_t idx1 = 0; idx1 < 3; ++idx1)
				{
					newterm.dmi[idx1] = tl2::var_to_str(newdmis[op_idx][idx1]);
					for(std::uint8_t idx2 = 0; idx2 < 3; ++idx2)
						newterm.Jgen[idx1][idx2] = tl2::var_to_str(newJgens[op_idx](idx1, idx2));
				}
				newterm.name += "_" + tl2::var_to_str(op_idx + 1);

				newterms.emplace_back(std::move(newterm));
			}
		}

		m_exchange_terms = std::move(newterms);
		RemoveDuplicateExchangeTerms();
		CalcExchangeTerms();
	}



	/**
	 * generate possible couplings up to a certain distance
	 */
	void GeneratePossibleExchangeTerms(
		t_real a, t_real b, t_real c,
		t_real alpha, t_real beta, t_real gamma,
		t_real dist_max, t_size _sc_max,
		std::optional<t_size> couplings_max)
	{
		// helper struct for finding possible couplings
		struct PossibleCoupling
		{
			// corresponding unit cell position
			t_vec_real pos1_uc{};
			t_vec_real pos2_uc{};

			// magnetic site position in supercell
			t_vec_real sc_vec{};
			t_vec_real pos2_sc{};

			// coordinates in orthogonal lab units
			t_vec_real pos1_uc_lab{};
			t_vec_real pos2_sc_lab{};

			// corresponding unit cell index
			t_size idx1_uc{};
			t_size idx2_uc{};

			// distance between the two magnetic sites
			t_real dist{};
		};

		CalcExternalField();
		CalcMagneticSites();
		CalcExchangeTerms();
		ExchangeTerms newterms;
		std::vector<PossibleCoupling> couplings;

		// crystal fractional coordinate trafo matrix
		t_mat_real A = tl2::A_matrix<t_mat_real>(a, b, c, alpha, beta, gamma);

		// generate a list of supercell vectors
		t_real sc_max = t_real(_sc_max);
		for(t_real sc_h = -sc_max; sc_h <= sc_max; sc_h += 1.)
		for(t_real sc_k = -sc_max; sc_k <= sc_max; sc_k += 1.)
		for(t_real sc_l = -sc_max; sc_l <= sc_max; sc_l += 1.)
		{
			// super cell vector
			const t_vec_real sc_vec = tl2::create<t_vec_real>({ sc_h, sc_k, sc_l });

			for(t_size idx1 = 0; idx1 < GetMagneticSitesCount()-1; ++idx1)
			{
				for(t_size idx2 = idx1+1; idx2 < GetMagneticSitesCount(); ++idx2)
				{
					PossibleCoupling coupling;

					coupling.idx1_uc = idx1;
					coupling.idx2_uc = idx2;

					coupling.pos1_uc = GetMagneticSite(idx1).pos_calc;
					coupling.pos2_uc = GetMagneticSite(idx2).pos_calc;

					coupling.sc_vec = sc_vec;
					coupling.pos2_sc = coupling.pos2_uc + sc_vec;

					// transform to lab units for correct distance
					coupling.pos1_uc_lab = A * coupling.pos1_uc;
					coupling.pos2_sc_lab = A * coupling.pos2_sc;

					coupling.dist = tl2::norm<t_vec_real>(
						coupling.pos2_sc_lab - coupling.pos1_uc_lab);
					if(coupling.dist <= dist_max)
						couplings.emplace_back(std::move(coupling));
				}
			}
		}

		// sort couplings by distance
		std::stable_sort(couplings.begin(), couplings.end(),
			[](const PossibleCoupling& coupling1, const PossibleCoupling& coupling2) -> bool
		{
			return coupling1.dist < coupling2.dist;
		});

		// add couplings to list
		t_size coupling_idx = 0;
		for(const PossibleCoupling& coupling : couplings)
		{
			if(couplings_max && coupling_idx >= *couplings_max)
				break;

			ExchangeTerm newterm{};
			newterm.site1_calc = coupling.idx1_uc;
			newterm.site2_calc = coupling.idx2_uc;
			newterm.site1 = GetMagneticSite(newterm.site1_calc).name;
			newterm.site2 = GetMagneticSite(newterm.site2_calc).name;
			newterm.dist_calc = coupling.sc_vec;
			newterm.dist[0] = tl2::var_to_str(newterm.dist_calc[0]);
			newterm.dist[1] = tl2::var_to_str(newterm.dist_calc[1]);
			newterm.dist[2] = tl2::var_to_str(newterm.dist_calc[2]);
			newterm.J = "0";
			newterm.name += "coupling_" + tl2::var_to_str(coupling_idx + 1);
			newterms.emplace_back(std::move(newterm));

			++coupling_idx;
		}

		m_exchange_terms = std::move(newterms);
		RemoveDuplicateExchangeTerms();
		CalcExchangeTerms();
	}



	void RemoveDuplicateMagneticSites()
	{
		for(auto iter1 = m_sites.begin(); iter1 != m_sites.end(); ++iter1)
		{
			for(auto iter2 = std::next(iter1, 1); iter2 != m_sites.end();)
			{
				if(tl2::equals<t_vec_real>(iter1->pos_calc, iter2->pos_calc, m_eps))
					iter2 = m_sites.erase(iter2);
				else
					std::advance(iter2, 1);
			}
		}
	}



	void RemoveDuplicateExchangeTerms()
	{
		for(auto iter1 = m_exchange_terms.begin(); iter1 != m_exchange_terms.end(); ++iter1)
		{
			for(auto iter2 = std::next(iter1, 1); iter2 != m_exchange_terms.end();)
			{
				bool same_uc = (iter1->site1 == iter2->site1 && iter1->site2 == iter2->site2);
				bool same_sc = tl2::equals<t_vec_real>(iter1->dist_calc, iter2->dist_calc, m_eps);

				if(same_uc && same_sc)
					iter2 = m_exchange_terms.erase(iter2);
				else
					std::advance(iter2, 1);
			}
		}
	}
	// --------------------------------------------------------------------



	// --------------------------------------------------------------------
	// calculation functions
	// --------------------------------------------------------------------
	/**
	 * calculate the rotation matrix for the external field
	 */
	void CalcExternalField()
	{
		bool use_field =
			(!tl2::equals_0<t_real>(m_field.mag, m_eps) || m_field.align_spins)
			&& m_field.dir.size() == 3;

		if(use_field)
		{
			// rotate field to [001] direction
			m_rot_field = tl2::convert<t_mat>(
				tl2::trans<t_mat_real>(
					tl2::rotation<t_mat_real, t_vec_real>(
						-m_field.dir, m_zdir, &m_rotaxis, m_eps)));

#ifdef __TLIBS2_MAGDYN_DEBUG_OUTPUT__
			std::cout << "Field rotation from:\n";
			tl2::niceprint(std::cout, -m_field.dir, 1e-4, 4);
			std::cout << "\nto:\n";
			tl2::niceprint(std::cout, m_zdir, 1e-4, 4);
			std::cout << "\nmatrix:\n";
			tl2::niceprint(std::cout, m_rot_field, 1e-4, 4);
			std::cout << std::endl;
#endif
		}
	}



	/**
	 * calculate the spin rotation trafo for the magnetic sites
	 * and parse any given expressions
	 */
	void CalcMagneticSite(MagneticSite& site)
	{
		try
		{
			tl2::ExprParser<t_cplx> parser = GetExprParser();

			// spin direction and orthogonal plane
			bool has_explicit_uv = true;

			// defaults
			site.pos_calc = tl2::zero<t_vec_real>(3);
			site.spin_dir_calc = tl2::zero<t_vec>(3);
			site.spin_ortho_calc = tl2::zero<t_vec>(3);
			site.spin_ortho_conj_calc = tl2::zero<t_vec>(3);
			if(site.g_e.size1() == 0 || site.g_e.size2() == 0)
				site.g_e = tl2::g_e<t_real> * tl2::unit<t_mat>(3);

			// spin magnitude
			if(parser.parse(site.spin_mag))
				site.spin_mag_calc = parser.eval().real();

			for(std::uint8_t idx = 0; idx < 3; ++idx)
			{
				// position
				if(site.pos[idx] != "")
				{
					if(parser.parse(site.pos[idx]))
					{
						site.pos_calc[idx] = parser.eval().real();
					}
					else
					{
						std::cerr << "Error parsing position \""
							<< site.pos[idx] << "\""
							<< " for site \"" << site.name << "\""
							<< " and component " << idx
							<< "." << std::endl;
					}
				}

				// spin direction
				if(site.spin_dir[idx] != "")
				{
					if(parser.parse(site.spin_dir[idx]))
					{
						site.spin_dir_calc[idx] = parser.eval();
					}
					else
					{
						std::cerr << "Error parsing spin direction \""
							<< site.spin_dir[idx] << "\""
							<< " for site \"" << site.name << "\""
							<< " and component " << idx
							<< "." << std::endl;
					}
				}

				// orthogonal spin direction
				if(site.spin_ortho[idx] != "")
				{
					if(parser.parse(site.spin_ortho[idx]))
					{
						site.spin_ortho_calc[idx] = parser.eval();
						site.spin_ortho_conj_calc[idx] = std::conj(site.spin_ortho_calc[idx]);
					}
					else
					{
						has_explicit_uv = false;

						std::cerr << "Error parsing spin orthogonal plane \""
							<< site.spin_ortho[idx] << "\""
							<< " for site \"" << site.name << "\""
							<< " and component " << idx
							<< "." << std::endl;
					}
				}
				else
				{
					has_explicit_uv = false;
				}
			}

			// spin rotation of equation (9) from (Toth 2015)
			if(m_field.align_spins)
			{
				std::tie(site.spin_ortho_calc, site.spin_dir_calc) = R_to_uv(m_rot_field);
			}
			else
			{
				if(!has_explicit_uv)
				{
					// calculate u and v from the spin rotation
					std::tie(site.spin_ortho_calc, site.spin_dir_calc) =
						spin_to_uv(site.spin_dir_calc);
				}

				// TODO: normalise the v vector as well as the real and imaginary u vectors
				// in case they are explicitly given

#ifdef __TLIBS2_MAGDYN_DEBUG_OUTPUT__
				std::cout << "Site " << site.name << " u = "
					<< site.spin_ortho_calc[0] << " " << site.spin_ortho_calc[1] << " " << site.spin_ortho_calc[2]
					<< std::endl;
				std::cout << "Site " << site.name << " v = "
					<< site.spin_dir_calc[0] << " " << site.spin_dir_calc[1] << " " << site.spin_dir_calc[2]
					<< std::endl;
#endif
			}

			site.spin_ortho_conj_calc = tl2::conj(site.spin_ortho_calc);
		}
		catch(const std::exception& ex)
		{
			std::cerr << "Error calculating site \"" << site.name << "\"."
				" Reason: " << ex.what() << std::endl;
		}
	}



	/**
	 * calculate the spin rotation trafo for the magnetic sites
	 * and parse any given expressions
	 */
	void CalcMagneticSites()
	{
		for(MagneticSite& site : GetMagneticSites())
			CalcMagneticSite(site);
	}



	/**
	 * parse the exchange term expressions
	 */
	void CalcExchangeTerm(ExchangeTerm& term)
	{
		try
		{
			tl2::ExprParser<t_cplx> parser = GetExprParser();

			// defaults
			term.dist_calc = tl2::zero<t_vec_real>(3);  // distance
			term.dmi_calc = tl2::zero<t_vec>(3);        // dmi interaction
			term.Jgen_calc = tl2::zero<t_mat>(3, 3);    // general exchange interaction

			// get site indices
			term.site1_calc = GetMagneticSiteIndex(term.site1);
			term.site2_calc = GetMagneticSiteIndex(term.site2);

			if(term.site1_calc >= GetMagneticSitesCount())
			{
				std::cerr << "Error: Unknown site 1 name \"" << term.site1 << "\"."
					<< " in coupling \"" << term.name << "\"."
					<< std::endl;
				return;
			}
			if(term.site2_calc >= GetMagneticSitesCount())
			{
				std::cerr << "Error: Unknown site 2 name \"" << term.site2 << "\"."
					<< " in coupling \"" << term.name << "\"."
					<< std::endl;
				return;
			}

			// symmetric interaction
			if(parser.parse(term.J))
			{
				term.J_calc = parser.eval();
			}
			else
			{
				std::cerr << "Error parsing J term \""
					<< term.J << "\"."
					<< std::endl;
			}

			for(std::uint8_t i = 0; i < 3; ++i)
			{
				// distance
				if(term.dist[i] != "")
				{
					if(parser.parse(term.dist[i]))
					{
						term.dist_calc[i] = parser.eval().real();
					}
					else
					{
						std::cerr << "Error parsing distance term \""
							<< term.dist[i]
							<< "\" (index " << i << ")"
							<< "." << std::endl;
					}
				}

				// dmi
				if(term.dmi[i] != "")
				{
					if(parser.parse(term.dmi[i]))
					{
						term.dmi_calc[i] = parser.eval();
					}
					else
					{
						std::cerr << "Error parsing DMI term \""
							<< term.dmi[i]
							<< "\" (index " << i << ")"
							<< "." << std::endl;
					}
				}

				// general exchange interaction
				for(std::uint8_t j = 0; j < 3; ++j)
				{
					if(term.Jgen[i][j] == "")
						continue;

					if(parser.parse(term.Jgen[i][j]))
					{
						term.Jgen_calc(i, j) = parser.eval();
					}
					else
					{
						std::cerr << "Error parsing general term \""
							<< term.Jgen[i][j]
							<< "\" (indices " << i << ", " << j << ")"
							<< "." << std::endl;
					}
				}
			}
		}
		catch(const std::exception& ex)
		{
			std::cerr << "Error calculating coupling \"" << term.name << "\"."
				" Reason: " << ex.what() << "."
				<< std::endl;
		}
	}



	/**
	 * parse all exchange term expressions
	 */
	void CalcExchangeTerms()
	{
		for(ExchangeTerm& term : GetExchangeTerms())
			CalcExchangeTerm(term);
	}



	/**
	 * calculate the real-space interaction matrix J of
	 * equations (10) - (13) from (Toth 2015)
	 */
	t_mat CalcRealJ(const ExchangeTerm& term) const
	{
		// symmetric part of the exchange interaction matrix, see (Toth 2015) p. 2
		t_mat J = tl2::diag<t_mat>(
			tl2::create<t_vec>({ term.J_calc, term.J_calc, term.J_calc }));

		// dmi as anti-symmetric part of interaction matrix
		// using a cross product matrix, see (Toth 2015) p. 2
		if(term.dmi_calc.size() == 3)
			J += tl2::skewsymmetric<t_mat, t_vec>(-term.dmi_calc);

		// general J matrix
		if(term.Jgen_calc.size1() == 3 && term.Jgen_calc.size2() == 3)
			J += term.Jgen_calc;

		// incommensurate case: rotation wrt magnetic unit cell
		// equations (21), (6), (2) as well as section 10 from (Toth 2015)
		if(IsIncommensurate())
		{
			const t_real rot_UC_angle =
				s_twopi * tl2::inner<t_vec_real>(m_ordering, term.dist_calc);

			if(!tl2::equals_0<t_real>(rot_UC_angle, m_eps))
			{
				t_mat rot_UC = tl2::convert<t_mat>(
					tl2::rotation<t_mat_real, t_vec_real>(
						m_rotaxis, rot_UC_angle));
				J = J * rot_UC;

#ifdef __TLIBS2_MAGDYN_DEBUG_OUTPUT__
				std::cout << "Coupling rot_UC = " << term.name << ":\n";
				tl2::niceprint(std::cout, rot_UC, 1e-4, 4);
#endif
			}
		}

		return J;
	}



	/**
	 * calculate the reciprocal interaction matrices J(Q) and J(-Q) of
	 * equations (12) and (14) from (Toth 2015)
	 */
	std::tuple<t_Jmap, t_Jmap> CalcReciprocalJs(const t_vec_real& Qvec) const
	{
		t_Jmap J_Q, J_Q0;

		// no (or not valid) exchange terms given
		if(GetExchangeTermsCount() == 0)
			return std::make_tuple(J_Q, J_Q0);

		// iterate couplings to pre-calculate corresponding J matrices
		for(const ExchangeTerm& term : GetExchangeTerms())
		{
			// insert or add an exchange matrix at the given site indices
			auto insert_or_add = [](t_Jmap& J, const t_indices& indices, const t_mat& J33)
			{
				if(auto iter = J.find(indices); iter != J.end())
					iter->second += J33;
				else
					J.emplace(std::move(std::make_pair(indices, J33)));
			};

			if(!CheckMagneticSite(term.site1_calc) || !CheckMagneticSite(term.site2_calc))
				continue;

			const t_indices indices = std::make_pair(term.site1_calc, term.site2_calc);
			const t_indices indices_t = std::make_pair(term.site2_calc, term.site1_calc);

			const t_mat J = CalcRealJ(term);
			if(J.size1() == 0 || J.size2() == 0)
				continue;
			const t_mat J_T = tl2::trans(J);

			// get J in reciprocal space by fourier trafo
			// equations (14), (12), (11), and (52) from (Toth 2015)
			const t_cplx phase = m_phase_sign * s_imag * s_twopi *
				tl2::inner<t_vec_real>(term.dist_calc, Qvec);

			insert_or_add(J_Q, indices, J * std::exp(phase));
			insert_or_add(J_Q, indices_t, J_T * std::exp(-phase));

			insert_or_add(J_Q0, indices, J);
			insert_or_add(J_Q0, indices_t, J_T);
		}  // end of iteration over couplings

		return std::make_tuple(J_Q, J_Q0);
	}



	/**
	 * get the hamiltonian at the given momentum
	 * @note implements the formalism given by (Toth 2015)
	 * @note a first version for a simplified ferromagnetic dispersion was based on (Heinsdorf 2021)
	 */
	t_mat CalcHamiltonian(const t_vec_real& Qvec) const
	{
		const t_size N = GetMagneticSitesCount();
		if(N == 0)
			return t_mat{};

		// build the interaction matrices J(Q) and J(-Q) of
		// equations (12) and (14) from (Toth 2015)
		const auto [J_Q, J_Q0] = CalcReciprocalJs(Qvec);

		// create the hamiltonian of equation (25) and (26) from (Toth 2015)
		t_mat A         = tl2::zero<t_mat>(N, N);
		t_mat A_conj_mQ = tl2::zero<t_mat>(N, N);
		t_mat B         = tl2::zero<t_mat>(N, N);
		t_mat C         = tl2::zero<t_mat>(N, N);

		bool use_field = !tl2::equals_0<t_real>(m_field.mag, m_eps)
			&& m_field.dir.size() == 3;

		// iterate magnetic sites
		for(t_size i = 0; i < N; ++i)
		{
			const MagneticSite& s_i = GetMagneticSite(i);

			// get the pre-calculated u and v vectors for the commensurate case
			const t_vec& u_i      = s_i.spin_ortho_calc;
			const t_vec& u_conj_i = s_i.spin_ortho_conj_calc;
			const t_vec& v_i      = s_i.spin_dir_calc;

			for(t_size j = 0; j < N; ++j)
			{
				const MagneticSite& s_j = GetMagneticSite(j);

				// get the pre-calculated u and v vectors for the commensurate case
				const t_vec& u_j      = s_j.spin_ortho_calc;
				const t_vec& u_conj_j = s_j.spin_ortho_conj_calc;
				const t_vec& v_j      = s_j.spin_dir_calc;

				// get the pre-calculated exchange matrices for the (i, j) coupling
				const t_indices indices_ij = std::make_pair(i, j);
				const t_mat* J_Q33 = nullptr;
				const t_mat* J_Q033 = nullptr;
				if(auto iter = J_Q.find(indices_ij); iter != J_Q.end())
					J_Q33 = &iter->second;
				if(auto iter = J_Q0.find(indices_ij); iter != J_Q0.end())
					J_Q033 = &iter->second;

				if(J_Q33)
				{
					const t_real S_mag = 0.5 * std::sqrt(s_i.spin_mag_calc * s_j.spin_mag_calc);

					// equation (26) from (Toth 2015)
					A(i, j)         = S_mag * tl2::inner_noconj<t_vec>(u_i,      (*J_Q33)  * u_conj_j);
					A_conj_mQ(i, j) = S_mag * tl2::inner_noconj<t_vec>(u_conj_i, (*J_Q33)  * u_j);
					B(i, j)         = S_mag * tl2::inner_noconj<t_vec>(u_i,      (*J_Q33)  * u_j);
				}

				if(J_Q033)
				{
					// equation (26) from (Toth 2015)
					C(i, i)        += s_j.spin_mag_calc * tl2::inner_noconj<t_vec>(v_i, (*J_Q033) * v_j);
				}
			}  // end of iteration over j sites

			// include external field, equation (28) from (Toth 2015)
			if(use_field)
			{
				const t_vec field = tl2::convert<t_vec>(-m_field.dir) * m_field.mag;
				const t_vec gv    = s_i.g_e * v_i;
				const t_cplx Bgv  = tl2::inner_noconj<t_vec>(field, gv);

				// bohr magneton in [meV/T]
				constexpr const t_real muB = tl2::mu_B<t_real>
					/ tl2::meV<t_real> * tl2::tesla<t_real>;

				A(i, i)         -= muB * Bgv;
				A_conj_mQ(i, i) -= std::conj(muB * Bgv);
			}
		}  // end of iteration over i sites

		// equation (25) from (Toth 2015)
		t_mat H = tl2::zero<t_mat>(N*2, N*2);
		tl2::set_submat(H, A - C,         0, 0);
		tl2::set_submat(H, B,             0, N);
		tl2::set_submat(H, tl2::herm(B),  N, 0);
		tl2::set_submat(H, A_conj_mQ - C, N, N);

		return H;
	}



	/**
	 * get the energies from a hamiltonian
	 * @note implements the formalism given by (Toth 2015)
	 */
	std::vector<EnergyAndWeight> CalcEnergiesFromHamiltonian(
		t_mat _H, const t_vec_real& Qvec,
		bool only_energies = false) const
	{
		const t_size N = GetMagneticSitesCount();
		if(N == 0 || _H.size1() == 0)
			return {};

		// equation (30) from (Toth 2015)
		t_mat g_sign = tl2::zero<t_mat>(N*2, N*2);
		for(t_size i = 0; i < N; ++i)
			g_sign(i, i) = 1.;
		for(t_size i = N; i < 2*N; ++i)
			g_sign(i, i) = -1.;

		// equation (31) from (Toth 2015)
		t_mat C_mat;
		t_size chol_try = 0;
		for(; chol_try<m_tries_chol; ++chol_try)
		{
			const auto [chol_ok, _C] = tl2_la::chol<t_mat>(_H);

			if(chol_ok)
			{
				C_mat = std::move(_C);
				break;
			}
			else
			{
				if(chol_try >= m_tries_chol-1)
				{
					using namespace tl2_ops;
					std::cerr << "Warning: Cholesky decomposition failed at Q = "
						<< Qvec << "." << std::endl;
					C_mat = std::move(_C);
					break;
				}

				// try forcing the hamilton to be positive definite
				for(t_size i = 0; i < 2*N; ++i)
					_H(i, i) += m_delta_chol;
			}
		}

		if(chol_try > 0)
		{
			using namespace tl2_ops;
			std::cerr << "Warning: Needed " << chol_try
				<< " corrections for cholesky decomposition at Q = "
				<< Qvec << "." << std::endl;
		}

		// see p. 5 in (Toth 2015)
		const t_mat H_mat = C_mat * g_sign * tl2::herm<t_mat>(C_mat);

		const bool is_herm = tl2::is_symm_or_herm<t_mat, t_real>(H_mat, m_eps);
		if(!is_herm)
		{
			using namespace tl2_ops;
			std::cerr << "Warning: Hamiltonian is not hermitian at Q = "
				<< Qvec << "." << std::endl;
		}

		// eigenvalues of the hamiltonian correspond to the energies
		// eigenvectors correspond to the spectral weights
		const auto [evecs_ok, evals, evecs] =
			tl2_la::eigenvec<t_mat, t_vec, t_cplx, t_real>(
				H_mat, only_energies, is_herm, true);
		if(!evecs_ok)
		{
			using namespace tl2_ops;
			std::cerr << "Warning: Eigensystem calculation failed at Q = "
				<< Qvec << "." << std::endl;
		}


		std::vector<EnergyAndWeight> energies_and_correlations{};
		energies_and_correlations.reserve(evals.size());

		// register energies
		for(const auto& eval : evals)
		{
			const EnergyAndWeight EandS { .E = eval.real(), };
			energies_and_correlations.emplace_back(std::move(EandS));
		}

		// weight factors
		if(!only_energies)
		{
			CalcCorrelationsFromHamiltonian(energies_and_correlations,
				H_mat, C_mat, g_sign, Qvec, evecs);
		}

		return energies_and_correlations;
	}



	/**
	 * get the dynamical structure factor from a hamiltonian
	 * @note implements the formalism given by (Toth 2015)
	 */
	void CalcCorrelationsFromHamiltonian(std::vector<EnergyAndWeight>& energies_and_correlations,
		const t_mat& H_mat, const t_mat& C_mat, const t_mat& g_sign,
		const t_vec_real& Qvec, const std::vector<t_vec>& evecs) const
	{
		const t_size N = GetMagneticSitesCount();
		if(N == 0)
			return;

		// get the sorting of the energies
		const std::vector<t_size> sorting = tl2::get_perm(
			energies_and_correlations.size(),
			[&energies_and_correlations](t_size idx1, t_size idx2) -> bool
		{
			return energies_and_correlations[idx1].E >=
				energies_and_correlations[idx2].E;
		});

		const t_mat evec_mat = tl2::create<t_mat>(tl2::reorder(evecs, sorting));
		const t_mat evec_mat_herm = tl2::herm(evec_mat);

		// equation (32) from (Toth 2015)
		const t_mat L_mat = evec_mat_herm * H_mat * evec_mat;    // energies
		t_mat E_sqrt = g_sign * L_mat;                           // abs. energies
		for(t_size i = 0; i < E_sqrt.size1(); ++i)
			E_sqrt(i, i) = std::sqrt(E_sqrt/*L_mat*/(i, i)); // sqrt. of abs. energies

		// re-create energies, to be consistent with the weights
		energies_and_correlations.clear();
		for(t_size i = 0; i < L_mat.size1(); ++i)
		{
			const EnergyAndWeight EandS
			{
				.E = L_mat(i, i).real(),
				.S = tl2::zero<t_mat>(3, 3),
				.S_perp = tl2::zero<t_mat>(3, 3),
			};

			energies_and_correlations.emplace_back(std::move(EandS));
		}

		const auto [C_inv, inv_ok] = tl2::inv(C_mat);
		if(!inv_ok)
		{
			using namespace tl2_ops;
			std::cerr << "Warning: Inversion failed at Q = "
				<< Qvec << "." << std::endl;
		}

		// equation (34) from (Toth 2015)
		const t_mat trafo = C_inv * evec_mat * E_sqrt;
		const t_mat trafo_herm = tl2::herm(trafo);

#ifdef __TLIBS2_MAGDYN_DEBUG_OUTPUT__
		t_mat D_mat = trafo_herm * H_mat * trafo;
		std::cout << "D = \n";
		tl2::niceprint(std::cout, D_mat, 1e-4, 4);
		std::cout << "\nE = \n";
		tl2::niceprint(std::cout, E_sqrt, 1e-4, 4);
		std::cout << "\nL = \n";
		tl2::niceprint(std::cout, L_mat, 1e-4, 4);
		std::cout << std::endl;
#endif

#ifdef __TLIBS2_MAGDYN_DEBUG_PY_OUTPUT__
		std::cout << "# --------------------------------------------------------------------------------\n";
		std::cout << "Y = np.zeros(3*3*4*4, dtype=complex).reshape((4,4,3,3))" << std::endl;
		std::cout << "V = np.zeros(3*3*4*4, dtype=complex).reshape((4,4,3,3))" << std::endl;
		std::cout << "Z = np.zeros(3*3*4*4, dtype=complex).reshape((4,4,3,3))" << std::endl;
		std::cout << "W = np.zeros(3*3*4*4, dtype=complex).reshape((4,4,3,3))" << std::endl;
#endif

		// building the spin correlation functions of equation (47) from (Toth 2015)
		for(std::uint8_t x_idx = 0; x_idx < 3; ++x_idx)
		for(std::uint8_t y_idx = 0; y_idx < 3; ++y_idx)
		{
			// equations (44) from (Toth 2015)
			t_mat V = tl2::create<t_mat>(N, N);
			t_mat W = tl2::create<t_mat>(N, N);
			t_mat Y = tl2::create<t_mat>(N, N);
			t_mat Z = tl2::create<t_mat>(N, N);

			for(t_size i = 0; i < N; ++i)
			for(t_size j = 0; j < N; ++j)
			{
				// get the sites
				const MagneticSite& s_i = GetMagneticSite(i);
				const MagneticSite& s_j = GetMagneticSite(j);

				// get the pre-calculated u vectors
				const t_vec& u_i = s_i.spin_ortho_calc;
				const t_vec& u_j = s_j.spin_ortho_calc;
				const t_vec& u_conj_i = s_i.spin_ortho_conj_calc;
				const t_vec& u_conj_j = s_j.spin_ortho_conj_calc;

				// pre-factors of equation (44) from (Toth 2015)
				const t_real S_mag = 4. * std::sqrt(s_i.spin_mag_calc * s_j.spin_mag_calc);
				const t_cplx phase = std::exp(-m_phase_sign * s_imag * s_twopi *
					tl2::inner<t_vec_real>(s_j.pos_calc - s_i.pos_calc, Qvec));

				// matrix elements of equation (44) from (Toth 2015)
				Y(i, j) = phase * S_mag * u_i[x_idx]      * u_conj_j[y_idx];
				V(i, j) = phase * S_mag * u_conj_i[x_idx] * u_conj_j[y_idx];
				Z(i, j) = phase * S_mag * u_i[x_idx]      * u_j[y_idx];
				W(i, j) = phase * S_mag * u_conj_i[x_idx] * u_j[y_idx];

#ifdef __TLIBS2_MAGDYN_DEBUG_PY_OUTPUT__
				std::cout << "Y[" << i << ", " << j << ", "
					<< x_idx << ", " << y_idx << "] = "
					<< Y(i, j).real() << " + " << Y(i, j).imag() << "j\n"
					<< "V[" << i << ", " << j << ", "
					<< x_idx << ", " << y_idx << "] = "
					<< V(i, j).real() << " + " << V(i, j).imag() << "j\n"
					<< "Z[" << i << ", " << j << ", "
					<< x_idx << ", " << y_idx << "] = "
					<< Z(i, j).real() << " + " << Z(i, j).imag() << "j\n"
					<< "W[" << i << ", " << j << ", "
					<< x_idx << ", " << y_idx << "] = "
					<< W(i, j).real() << " + " << W(i, j).imag() << "j"
					<< std::endl;
#endif
			} // end of iteration over sites

			// equation (47) from (Toth 2015)
			t_mat M = tl2::create<t_mat>(N*2, N*2);
			tl2::set_submat(M, Y, 0, 0);
			tl2::set_submat(M, V, N, 0);
			tl2::set_submat(M, Z, 0, N);
			tl2::set_submat(M, W, N, N);

			const t_mat M_trafo = trafo_herm * M * trafo;

#ifdef __TLIBS2_MAGDYN_DEBUG_OUTPUT__
			std::cout << "M_trafo for x=" << x_idx << ", y=" << y_idx << ":\n";
			tl2::niceprint(std::cout, M_trafo, 1e-4, 4);
			std::cout << std::endl;
#endif

			for(t_size i = 0; i < energies_and_correlations.size(); ++i)
			{
				(energies_and_correlations[i].S)(x_idx, y_idx) +=
					M_trafo(i, i) / t_real(2*N);
			}
		} // end of coordinate iteration

#ifdef __TLIBS2_MAGDYN_DEBUG_PY_OUTPUT__
			std::cout << "# --------------------------------------------------------------------------------\n"
				<< std::endl;
#endif
	}



	/**
	 * applies projectors and weight factors to get neutron intensities
	 * @note implements the formalism given by (Toth 2015)
	 */
	void CalcIntensities(const t_vec_real& Qvec, std::vector<EnergyAndWeight>&
		energies_and_correlations) const
	{
		for(EnergyAndWeight& E_and_S : energies_and_correlations)
		{
			// apply bose factor
			if(m_temperature >= 0.)
				E_and_S.S *= tl2::bose_cutoff(E_and_S.E, m_temperature, m_bose_cutoff);

			// apply orthogonal projector for magnetic neutron scattering,
			// see (Shirane 2002), p. 37, equation (2.64)
			//t_vec bragg_rot = use_field ? m_rot_field * m_bragg : m_bragg;
			//proj_neutron = tl2::ortho_projector<t_mat, t_vec>(bragg_rot, false);
			t_mat proj_neutron = tl2::ortho_projector<t_mat, t_vec>(Qvec, false);
			E_and_S.S_perp = proj_neutron * E_and_S.S * proj_neutron;

			// weights
			E_and_S.weight      = std::abs(tl2::trace<t_mat>(E_and_S.S_perp).real());
			E_and_S.weight_full = std::abs(tl2::trace<t_mat>(E_and_S.S).real());

			// polarisation channels
			for(std::uint8_t i = 0; i < 3; ++i)
			{
				const t_mat pol   = get_polarisation<t_mat>(i, false);
				const t_mat Sperp = pol * E_and_S.S_perp;
				const t_mat S     = pol * E_and_S.S;

				E_and_S.weight_channel[i] = std::abs(tl2::trace<t_mat>(Sperp).real());
				E_and_S.weight_channel_full[i] = std::abs(tl2::trace<t_mat>(S).real());
			}
		}
	}



	/**
	 * unite degenerate energies and their corresponding eigenstates
	 */
	std::vector<EnergyAndWeight> UniteEnergies(const std::vector<EnergyAndWeight>&
		energies_and_correlations) const
	{
		std::vector<EnergyAndWeight> new_energies_and_correlations{};
		new_energies_and_correlations.reserve(energies_and_correlations.size());

		for(const auto& curState : energies_and_correlations)
		{
			const t_real curE = curState.E;

			auto iter = std::find_if(
				new_energies_and_correlations.begin(),
				new_energies_and_correlations.end(),
				[curE, this](const auto& E_and_S) -> bool
			{
				t_real E = E_and_S.E;
				return tl2::equals<t_real>(E, curE, m_eps);
			});

			if(iter == new_energies_and_correlations.end())
			{
				// energy not yet seen
				new_energies_and_correlations.push_back(curState);
			}
			else
			{
				// energy already seen: add correlation matrices and weights
				iter->S      += curState.S;
				iter->S_perp += curState.S_perp;

				iter->weight      += curState.weight;
				iter->weight_full += curState.weight_full;

				for(std::uint8_t i = 0; i < 3; ++i)
				{
					iter->weight_channel[i]      += curState.weight_channel[i];
					iter->weight_channel_full[i] += curState.weight_channel_full[i];
				}
			}
		}

		return new_energies_and_correlations;
	}



	/**
	 * get the energies and the spin-correlation at the given momentum
	 * (also calculates incommensurate contributions and applies weight factors)
	 * @note implements the formalism given by (Toth 2015)
	 */
	std::vector<EnergyAndWeight> CalcEnergies(const t_vec_real& Qvec,
		bool only_energies = false) const
	{
		std::vector<EnergyAndWeight> EandWs;
		if(m_calc_H)
		{
			const t_mat H = CalcHamiltonian(Qvec);
			EandWs = CalcEnergiesFromHamiltonian(H, Qvec, only_energies);
		}

		if(IsIncommensurate())
		{
			// equations (39) and (40) from (Toth 2015)
			const t_mat proj_norm = tl2::convert<t_mat>(
				tl2::projector<t_mat_real, t_vec_real>(
					m_rotaxis, true));

			t_mat rot_incomm = tl2::unit<t_mat>(3);
			rot_incomm -= s_imag * m_phase_sign * tl2::skewsymmetric<t_mat, t_vec>(m_rotaxis);
			rot_incomm -= proj_norm;
			rot_incomm *= 0.5;

			const t_mat rot_incomm_conj = tl2::conj(rot_incomm);

			std::vector<EnergyAndWeight> EandWs_p, EandWs_m;

			if(m_calc_Hp)
			{
				const t_mat H_p = CalcHamiltonian(Qvec + m_ordering);
				EandWs_p = CalcEnergiesFromHamiltonian(
					H_p, Qvec + m_ordering, only_energies);
			}

			if(m_calc_Hm)
			{
				const t_mat H_m = CalcHamiltonian(Qvec - m_ordering);
				EandWs_m = CalcEnergiesFromHamiltonian(
					H_m, Qvec - m_ordering, only_energies);
			}

			if(!only_energies)
			{
				// formula 40 from (Toth 2015)
				for(EnergyAndWeight& EandW : EandWs)
					EandW.S = EandW.S * proj_norm;
				for(EnergyAndWeight& EandW : EandWs_p)
					EandW.S = EandW.S * rot_incomm;
				for(EnergyAndWeight& EandW : EandWs_m)
					EandW.S = EandW.S * rot_incomm_conj;
			}

			// unite energies and weights
			for(EnergyAndWeight& EandW : EandWs_p)
				EandWs.emplace_back(std::move(EandW));
			for(EnergyAndWeight& EandW : EandWs_m)
				EandWs.emplace_back(std::move(EandW));
		}

		if(!only_energies)
			CalcIntensities(Qvec, EandWs);

		if(m_unite_degenerate_energies)
			EandWs = UniteEnergies(EandWs);

		return EandWs;
	}



	std::vector<EnergyAndWeight> CalcEnergies(t_real h, t_real k, t_real l,
		bool only_energies = false) const
	{
		// momentum transfer
		const t_vec_real Qvec = tl2::create<t_vec_real>({ h, k, l });
		return CalcEnergies(Qvec, only_energies);
	}



	/**
	 * get the energy minimum
	 * @note a first version for a simplified ferromagnetic dispersion was based on (Heinsdorf 2021)
	 */
	t_real CalcMinimumEnergy() const
	{
		// energies at (000)
		const auto energies_and_correlations = CalcEnergies(0., 0., 0., true);

		// get minimum
		const auto min_iter = std::min_element(
			energies_and_correlations.begin(), energies_and_correlations.end(),
			[](const auto& E_and_S_1, const auto& E_and_S_2) -> bool
		{
			return std::abs(E_and_S_1.E) < std::abs(E_and_S_2.E);
		});

		if(min_iter == energies_and_correlations.end())
			return 0.;
		return min_iter->E;
	}



	/**
	 * get the ground-state energy
	 * @note zero-operator term in expansion of equation (20) in (Toth 2015)
	 */
	t_real CalcGroundStateEnergy() const
	{
		t_real E = 0.;

		for(const ExchangeTerm& term : GetExchangeTerms())
		{
			// check if the site indices are valid
			if(!CheckMagneticSite(term.site1_calc) || !CheckMagneticSite(term.site2_calc))
				continue;

			const MagneticSite& s_i = GetMagneticSite(term.site1_calc);
			const MagneticSite& s_j = GetMagneticSite(term.site2_calc);

			const t_mat J = CalcRealJ(term);  // Q=0 -> no rotation needed

			const t_vec Si = s_i.spin_mag_calc * s_i.spin_dir_calc;
			const t_vec Sj = s_j.spin_mag_calc * s_j.spin_dir_calc;

			E += tl2::inner_noconj<t_vec>(Si, J * Sj).real();
		}

		return E;
	}
	// --------------------------------------------------------------------



	// --------------------------------------------------------------------
	// loading and saving
	// --------------------------------------------------------------------
	/**
	 * generates the dispersion plot along the given q path
	 */
	void SaveDispersion(const std::string& filename,
		t_real h_start, t_real k_start, t_real l_start,
		t_real h_end, t_real k_end, t_real l_end,
		t_size num_qs = 128) const
	{
		std::ofstream ofstr{filename};
		SaveDispersion(ofstr, h_start, k_start, l_start, h_end, k_end, l_end, num_qs);
	}



	/**
	 * generates the dispersion plot along the given q path
	 */
	void SaveDispersion(std::ostream& ostr,
		t_real h_start, t_real k_start, t_real l_start,
		t_real h_end, t_real k_end, t_real l_end,
		t_size num_qs = 128) const
	{
		ostr.precision(m_prec);

		ostr
			<< std::setw(m_prec*2) << std::left << "# h"
			<< std::setw(m_prec*2) << std::left << "k"
			<< std::setw(m_prec*2) << std::left << "l"
			<< std::setw(m_prec*2) << std::left << "E"
			<< std::setw(m_prec*2) << std::left << "w"
			<< std::setw(m_prec*2) << std::left << "w_sf1"
			<< std::setw(m_prec*2) << std::left << "w_sf2"
			<< std::setw(m_prec*2) << std::left << "w_nsf"
			<< std::endl;

		for(t_size i = 0; i < num_qs; ++i)
		{
			// get Q
			const t_real h = std::lerp(h_start, h_end, t_real(i)/t_real(num_qs-1));
			const t_real k = std::lerp(k_start, k_end, t_real(i)/t_real(num_qs-1));
			const t_real l = std::lerp(l_start, l_end, t_real(i)/t_real(num_qs-1));

			// get E and S(Q, E)
			const auto energies_and_correlations = CalcEnergies(h, k, l, false);
			for(const auto& E_and_S : energies_and_correlations)
			{
				ostr
					<< std::setw(m_prec*2) << std::left << h
					<< std::setw(m_prec*2) << std::left << k
					<< std::setw(m_prec*2) << std::left << l
					<< std::setw(m_prec*2) << E_and_S.E
					<< std::setw(m_prec*2) << E_and_S.weight
					<< std::setw(m_prec*2) << E_and_S.weight_channel[0]
					<< std::setw(m_prec*2) << E_and_S.weight_channel[1]
					<< std::setw(m_prec*2) << E_and_S.weight_channel[2]
					<< std::endl;
			}
		}
	}



	/**
	 * load a configuration from a file
	 */
	bool Load(const std::string& filename)
	{
		try
		{
			// properties tree
			boost::property_tree::ptree node;

			// read xml file
			std::ifstream ifstr{filename};
			boost::property_tree::read_xml(ifstr, node);

			// check signature
			if(auto optInfo = node.get_optional<std::string>("magdyn.meta.info");
				!optInfo || !(*optInfo==std::string{"magdyn_tool"}))
			{
				return false;
			}

			const auto &magdyn = node.get_child("magdyn");
			return Load(magdyn);
		}
		catch(const std::exception& ex)
		{
			std::cerr << "Error: Could not load \"" << filename << "\"."
				<< " Reason: " << ex.what()
				<< std::endl;

			return false;
		}
	}



	/**
	 * save the configuration to a file
	 */
	bool Save(const std::string& filename) const
	{
		// properties tree
		boost::property_tree::ptree node;

		// write signature
		node.put<std::string>("meta.info", "magdyn_tool");
		node.put<std::string>("meta.date",
			tl2::epoch_to_str<t_real>(tl2::epoch<t_real>()));

		if(!Save(node))
			return false;

		boost::property_tree::ptree root_node;
		root_node.put_child("magdyn", node);

		// write xml file
		std::ofstream ofstr{filename};
		if(!ofstr)
			return false;

		ofstr.precision(m_prec);
		boost::property_tree::write_xml(ofstr, root_node,
			boost::property_tree::xml_writer_make_settings(
				'\t', 1, std::string{"utf-8"}));
		return true;
	}



	/**
	 * load a configuration from a property tree
	 */
	bool Load(const boost::property_tree::ptree& node)
	{
		Clear();

		// variables
		if(auto vars = node.get_child_optional("variables"); vars)
		{
			for(const auto &var : *vars)
			{
				auto name = var.second.get_optional<std::string>("name");
				if(!name)
					continue;

				Variable variable;
				variable.name = *name;
				variable.value = var.second.get<t_cplx>("value", 0.);

				AddVariable(std::move(variable));
			}
		}

		// magnetic sites
		if(auto sites = node.get_child_optional("atom_sites"); sites)
		{
			std::unordered_set<std::string> seen_names;
			t_size unique_name_counter = 1;

			for(const auto &site : *sites)
			{
				MagneticSite magnetic_site;

				magnetic_site.name = site.second.get<std::string>("name", "");
				if(magnetic_site.name == "")
					magnetic_site.name = "site_" + tl2::var_to_str(GetMagneticSitesCount());

				if(seen_names.find(magnetic_site.name) != seen_names.end())
				{
					// try to create a unique name
					magnetic_site.name += "_" + tl2::var_to_str(unique_name_counter);
					++unique_name_counter;
				}
				else
				{
					seen_names.insert(magnetic_site.name);
				}

				magnetic_site.pos_calc = tl2::create<t_vec_real>(
				{
					site.second.get<t_real>("position_x", 0.),
					site.second.get<t_real>("position_y", 0.),
					site.second.get<t_real>("position_z", 0.),
				});

				magnetic_site.pos[0] = site.second.get<std::string>("position_x", "0");
				magnetic_site.pos[1] = site.second.get<std::string>("position_y", "0");
				magnetic_site.pos[2] = site.second.get<std::string>("position_z", "0");

				magnetic_site.spin_dir[0] = site.second.get<std::string>("spin_x", "0");
				magnetic_site.spin_dir[1] = site.second.get<std::string>("spin_y", "0");
				magnetic_site.spin_dir[2] = site.second.get<std::string>("spin_z", "1");

				magnetic_site.spin_ortho[0] = site.second.get<std::string>("spin_ortho_x", "");
				magnetic_site.spin_ortho[1] = site.second.get<std::string>("spin_ortho_y", "");
				magnetic_site.spin_ortho[2] = site.second.get<std::string>("spin_ortho_z", "");

				magnetic_site.spin_mag = site.second.get<std::string>("spin_magnitude", "1");

				if(magnetic_site.g_e.size1() == 0 || magnetic_site.g_e.size2() == 0)
					magnetic_site.g_e = tl2::g_e<t_real> * tl2::unit<t_mat>(3);

				AddMagneticSite(std::move(magnetic_site));
			}
		}

		// exchange terms / couplings
		if(auto terms = node.get_child_optional("exchange_terms"); terms)
		{
			std::unordered_set<std::string> seen_names;
			t_size unique_name_counter = 1;

			for(const auto &term : *terms)
			{
				ExchangeTerm exchange_term;

				exchange_term.name = term.second.get<std::string>("name", "");
				if(exchange_term.name == "")
					exchange_term.name = "coupling_" + tl2::var_to_str(GetExchangeTermsCount());

				if(seen_names.find(exchange_term.name) != seen_names.end())
				{
					// try to create a unique name
					exchange_term.name += "_" + tl2::var_to_str(unique_name_counter);
					++unique_name_counter;
				}
				else
				{
					seen_names.insert(exchange_term.name);
				}

				exchange_term.site1_calc = term.second.get<t_size>("atom_1_index", 0);
				exchange_term.site2_calc = term.second.get<t_size>("atom_2_index", 0);

				if(auto name1 = term.second.get_optional<std::string>("atom_1_name"); name1)
				{
					// get the magnetic site index via the name
					if(auto sites1 = FindMagneticSites(*name1); sites1.size() == 1)
						exchange_term.site1 = sites1[0]->name;
					else
					{
						std::cerr
							<< "Error in coupling \"" << exchange_term.name
							<< "\": Site 1 name \"" << *name1
							<< "\" was not found." << std::endl;
					}
				}
				else
				{
					// get the magnetic site name via the index
					exchange_term.site1 = GetMagneticSite(exchange_term.site1_calc).name;
				}

				if(auto name2 = term.second.get_optional<std::string>("atom_2_name"); name2)
				{
					if(auto sites2 = FindMagneticSites(*name2); sites2.size() == 1)
						exchange_term.site2 = sites2[0]->name;
					else
					{
						std::cerr
							<< "Error in coupling \"" << exchange_term.name
							<< "\": Site 2 name \"" << *name2
							<< "\" was not found." << std::endl;
					}
				}
				else
				{
					// get the magnetic site name via the index
					exchange_term.site2 = GetMagneticSite(exchange_term.site2_calc).name;
				}

				exchange_term.dist_calc = tl2::create<t_vec_real>(
				{
					term.second.get<t_real>("distance_x", 0.),
					term.second.get<t_real>("distance_y", 0.),
					term.second.get<t_real>("distance_z", 0.),
				});

				exchange_term.dist[0] = term.second.get<std::string>("distance_x", "0");
				exchange_term.dist[1] = term.second.get<std::string>("distance_y", "0");
				exchange_term.dist[2] = term.second.get<std::string>("distance_z", "0");

				exchange_term.J = term.second.get<std::string>("interaction", "0");

				static const std::array<std::string, 3> comps{{"x", "y", "z"}};
				for(std::uint8_t i = 0; i < 3; ++i)
				{
					exchange_term.dmi[i] = term.second.get<std::string>(
						std::string("dmi_") + comps[i], "0");

					for(std::uint8_t j = 0; j < 3; ++j)
					{
						exchange_term.Jgen[i][j] = term.second.get<std::string>(
							std::string("gen_") + comps[i] + comps[j], "0");
					}
				}

				AddExchangeTerm(std::move(exchange_term));
			}
		}

		// external field
		if(auto field = node.get_child_optional("field"); field)
		{
			ExternalField thefield;

			thefield.mag = 0.;
			thefield.align_spins = false;

			thefield.dir = tl2::create<t_vec_real>(
			{
				field->get<t_real>("direction_h", 0.),
				field->get<t_real>("direction_k", 0.),
				field->get<t_real>("direction_l", 1.),
			});

			if(auto optVal = field->get_optional<t_real>("magnitude"))
				thefield.mag = *optVal;

			if(auto optVal = field->get_optional<bool>("align_spins"))
				thefield.align_spins = *optVal;

			SetExternalField(thefield);
		}

		// temperature
		m_temperature = node.get<t_real>("temperature", -1.);

		// ordering vector
		if(auto ordering = node.get_child_optional("ordering"); ordering)
		{
			t_vec_real ordering_vec = tl2::create<t_vec_real>(
			{
				ordering->get<t_real>("h", 0.),
				ordering->get<t_real>("k", 0.),
				ordering->get<t_real>("l", 0.),
			});

			SetOrderingWavevector(ordering_vec);
		}

		// rotation axis
		if(auto axis = node.get_child_optional("rotation_axis"); axis)
		{
			t_vec_real rotaxis = tl2::create<t_vec_real>(
			{
				axis->get<t_real>("h", 1.),
				axis->get<t_real>("k", 0.),
				axis->get<t_real>("l", 0.),
			});

			SetRotationAxis(rotaxis);
		}

		CalcExternalField();
		CalcMagneticSites();
		CalcExchangeTerms();
		return true;
	}



	/**
	 * save the configuration to a property tree
	 */
	bool Save(boost::property_tree::ptree& node) const
	{
		// external field
		if(m_field.dir.size() == 3)
		{
			node.put<t_real>("field.direction_h", m_field.dir[0]);
			node.put<t_real>("field.direction_k", m_field.dir[1]);
			node.put<t_real>("field.direction_l", m_field.dir[2]);
		}
		node.put<t_real>("field.magnitude", m_field.mag);
		node.put<bool>("field.align_spins", m_field.align_spins);

		// ordering vector
		if(m_ordering.size() == 3)
		{
			node.put<t_real>("ordering.h", m_ordering[0]);
			node.put<t_real>("ordering.k", m_ordering[1]);
			node.put<t_real>("ordering.l", m_ordering[2]);
		}

		// rotation axis
		if(m_rotaxis.size() == 3)
		{
			node.put<t_real>("rotation_axis.h", m_rotaxis[0]);
			node.put<t_real>("rotation_axis.k", m_rotaxis[1]);
			node.put<t_real>("rotation_axis.l", m_rotaxis[2]);
		}

		// temperature
		node.put<t_real>("temperature", m_temperature);

		// variables
		for(const auto& var : GetVariables())
		{
			boost::property_tree::ptree itemNode;
			itemNode.put<std::string>("name", var.name);
			itemNode.put<t_cplx>("value", var.value);

			node.add_child("variables.variable", itemNode);
		}

		// magnetic sites
		for(const auto& site : GetMagneticSites())
		{
			boost::property_tree::ptree itemNode;
			itemNode.put<std::string>("name", site.name);

			itemNode.put<std::string>("position_x", site.pos[0]);
			itemNode.put<std::string>("position_y", site.pos[1]);
			itemNode.put<std::string>("position_z", site.pos[2]);

			itemNode.put<std::string>("spin_x", site.spin_dir[0]);
			itemNode.put<std::string>("spin_y", site.spin_dir[1]);
			itemNode.put<std::string>("spin_z", site.spin_dir[2]);

			itemNode.put<std::string>("spin_ortho_x", site.spin_ortho[0]);
			itemNode.put<std::string>("spin_ortho_y", site.spin_ortho[1]);
			itemNode.put<std::string>("spin_ortho_z", site.spin_ortho[2]);

			itemNode.put<std::string>("spin_magnitude", site.spin_mag);

			node.add_child("atom_sites.site", itemNode);
		}

		// exchange terms
		for(const auto& term : GetExchangeTerms())
		{
			boost::property_tree::ptree itemNode;
			itemNode.put<std::string>("name", term.name);

			// save the magnetic site names and indices
			itemNode.put<t_size>("atom_1_index", term.site1_calc);
			itemNode.put<t_size>("atom_2_index", term.site2_calc);
			itemNode.put<std::string>("atom_1_name", term.site1);
			itemNode.put<std::string>("atom_2_name", term.site2);

			itemNode.put<std::string>("distance_x", term.dist[0]);
			itemNode.put<std::string>("distance_y", term.dist[1]);
			itemNode.put<std::string>("distance_z", term.dist[2]);

			itemNode.put<std::string>("interaction", term.J);

			static const std::array<std::string, 3> comps{{"x", "y", "z"}};
			for(std::uint8_t i = 0; i < 3; ++i)
			{
				itemNode.put<std::string>(std::string("dmi_") +
					comps[i], term.dmi[i]);

				for(std::uint8_t j = 0; j < 3; ++j)
				{
					itemNode.put<std::string>(std::string("gen_") +
						comps[i] + comps[j], term.Jgen[i][j]);
				}
			}

			node.add_child("exchange_terms.term", itemNode);
		}

		return true;
	}
	// --------------------------------------------------------------------



protected:
	/**
	 * converts the rotation matrix rotating the local spins to ferromagnetic
	 * [001] directions into the vectors comprised of the matrix columns
	 * @see equation (9) and (51) from (Toth 2015)
	 */
	std::tuple<t_vec, t_vec> R_to_uv(const t_mat& R)
	{
		const t_vec u = tl2::col<t_mat, t_vec>(R, 0)
			 + s_imag * tl2::col<t_mat, t_vec>(R, 1);
		const t_vec v = tl2::col<t_mat, t_vec>(R, 2);

		return std::make_tuple(u, v);
	}



	/**
	 * rotate local spin to ferromagnetic [001] direction
	 * @see equations (7) and (9) from (Toth 2015)
	 */
	std::tuple<t_vec, t_vec> spin_to_uv(const t_vec& spin_dir)
	{
		const auto [spin_re, spin_im] = tl2::split_cplx<t_vec, t_vec_real>(spin_dir);

		if(!tl2::equals_0<t_vec_real>(spin_im, m_eps))
			std::cerr << "Warning: Spin vector should be purely real." << std::endl;

		// only use real part, imaginary part should be zero
		//spin_re /= tl2::norm<t_vec_real>(spin_re);
		const t_mat_real _rot = tl2::rotation<t_mat_real, t_vec_real>(
			spin_re, m_zdir, &m_rotaxis, m_eps);

		const t_mat rot = tl2::convert<t_mat, t_mat_real>(_rot);
		return R_to_uv(rot);
	}



private:
	// magnetic sites
	MagneticSites m_sites{};

	// magnetic couplings
	ExchangeTerms m_exchange_terms{};

	// open variables in expressions
	std::vector<Variable> m_variables{};

	// external field
	ExternalField m_field{};
	// matrix to rotate field into the [001] direction
	t_mat m_rot_field{ tl2::unit<t_mat>(3) };

	// ordering wave vector for incommensurate structures
	t_vec_real m_ordering{ tl2::zero<t_vec_real>(3) };
	t_vec_real m_rotaxis{ tl2::create<t_vec_real>({ 1., 0., 0. }) };

	// calculate the hamiltonian for Q, Q+ordering, and Q-ordering
	bool m_calc_H{ true };
	bool m_calc_Hp{ true };
	bool m_calc_Hm{ true };

	// direction to rotation spins into, usually [001]
	t_vec_real m_zdir{ tl2::create<t_vec_real>({ 0., 0., 1. }) };

	// temperature (-1: disable bose factor)
	t_real m_temperature{ -1. };

	// bose cutoff energy to avoid infinities
	t_real m_bose_cutoff{ 0.025 };

	// settings
	bool m_is_incommensurate{ false };
	bool m_force_incommensurate{ false };
	bool m_unite_degenerate_energies{ true };

	// settings for cholesky decomposition
	t_size m_tries_chol{ 50 };
	t_real m_delta_chol{ 0.0025 };

	// precisions
	t_real m_eps{ 1e-6 };
	int m_prec{ 6 };

	// conventions
	t_real m_phase_sign{ -1. };

	// constants
	static constexpr const t_cplx s_imag { t_real(0), t_real(1) };
	static constexpr const t_real s_twopi { t_real(2)*tl2::pi<t_real> };
};

}
#endif
