/********************************************************************\
 * gnc-option.hpp -- Application options system                     *
 * Copyright (C) 2019 John Ralls <jralls@ceridwen.us>               *
 *                                                                  *
 * This program is free software; you can redistribute it and/or    *
 * modify it under the terms of the GNU General Public License as   *
 * published by the Free Software Foundation; either version 2 of   *
 * the License, or (at your option) any later version.              *
 *                                                                  *
 * This program is distributed in the hope that it will be useful,  *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of   *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    *
 * GNU General Public License for more details.                     *
 *                                                                  *
 * You should have received a copy of the GNU General Public License*
 * along with this program; if not, contact:                        *
 *                                                                  *
 * Free Software Foundation           Voice:  +1-617-542-5942       *
 * 51 Franklin Street, Fifth Floor    Fax:    +1-617-542-2652       *
 * Boston, MA  02110-1301,  USA       gnu@gnu.org                   *
 *                                                                  *
\********************************************************************/

#ifndef GNC_OPTION_HPP_
#define GNC_OPTION_HPP_

extern "C"
{
#include <config.h>
#include <qof.h>
#include <gnc-budget.h>
#include <gnc-commodity.h>
}
#include <gnc-datetime.hpp>
#include <gnc-numeric.hpp>
#include <guid.hpp>
#include <libguile.h>
#include <string>
#include <utility>
#include <vector>
#include <exception>
#include <functional>
#include <variant>

/*
 * Unused base class to document the structure of the current Scheme option
 * vector, re-expressed in C++. The comment-numbers on the right indicate which
 * item in the Scheme vector each item implements.
 *
 * Not everything here needs to be implemented, nor will it necessarily be
 * implemented the same way. For example, part of the purpose of this redesign
 * is to convert from saving options as strings of Scheme code to some form of
 * key-value pair in the book options, so generate_restore_form() will likely be
 * supplanted with save_to_book().

template <typename ValueType>
class GncOptionBase
{
public:
    virtual ~GncOption = default;
    virtual ValueType get_value() const = 0;                             //5
    virtual ValueType get_default_value() = 0;
    virtual SCM get_SCM_value() = 0;
    virtual SCM get_SCM_default_value() const = 0;                       //7
    virtual void set_value(ValueType) = 0;                               //6
// generate_restore_form outputs a Scheme expression (a "form") that finds an
// option and sets it to the current value. e.g.:
//(let ((option (gnc:lookup-option options
//                                 "Display"
//                                 "Amount")))
//  ((lambda (option) (if option ((gnc:option-setter option) 'none))) option))
// it uses gnc:value->string to generate the "'none" (or whatever the option's
// value would be as input to the scheme interpreter).

    virtual std::string generate_restore_form();                         //8
    virtual void save_to_book(QofBook*) const noexcept;                  //9
    virtual void read_from_book(QofBook*);                               //10
    virtual std::vector<std::string> get_option_strings();               //15
    virtual set_changed_callback(std::function<void(void*)>);            //14
protected:
    const std::string m_section;                                         //0
    const std::string m_name;                                            //1
    const std::string m_sort_tag;                                        //2
    const std::type_info m_kvp_type;                                     //3
    const std::string m_doc_string;                                      //4
    std::function<void(void*)> m_changed_callback;   //Part of the make-option closure
    std::function<void(void*)>m_option_widget_changed_callback;          //16
};
*/

enum GncOptionUIType
{
    INTERNAL,
    BOOLEAN,
    STRING,
    TEXT,
    CURRENCY,
    COMMODITY,
    MULTICHOICE,
    DATE,
    ACCOUNT_LIST,
    ACCOUNT_SEL,
    LIST,
    NUMBER_RANGE,
    COLOR,
    FONT,
    BUDGET,
    PIXMAP,
    RADIOBUTTON,
    DATE_FORMAT,
    OWNER,
    CUSTOMER,
    VENDOR,
    EMPLOYEE,
    INVOICE,
    TAX_TABLE,
    QUERY,
};

struct OptionClassifier
{
    std::string m_section;
    std::string m_name;
    std::string m_sort_tag;
//  std::type_info m_kvp_type;
    std::string m_doc_string;
};

class GncOptionUIItem;

/**
 * Holds a pointer to the UI item which will control the option and an enum
 * representing the type of the option for dispatch purposes; all of that
 * happens in gnucash/gnome-utils/dialog-options and
 * gnucash/gnome/business-option-gnome.
 *
 * This class takes no ownership responsibility, so calling code is responsible
 * for ensuring that the UI_Item is alive. For convenience the public
 * clear_ui_item function can be used as a weak_ptr's destruction callback to
 * ensure that the ptr will be nulled if the ui_item is destroyed elsewhere.
 */
class OptionUIItem
{
public:
    GncOptionUIType get_ui_type() const { return m_ui_type; }
    GncOptionUIItem* const get_ui_item() const {return m_ui_item; }
    void clear_ui_item() { m_ui_item = nullptr; }
    void set_ui_item(GncOptionUIItem* ui_item)
    {
        if (m_ui_type == GncOptionUIType::INTERNAL)
        {
            std::string error{"INTERNAL option, setting the UI item forbidden."};
            throw std::logic_error(std::move(error));
        }
        m_ui_item = ui_item;
    }
    void make_internal()
    {
        if (m_ui_item != nullptr)
        {
            std::string error("Option has a UI Element, can't be INTERNAL.");
            throw std::logic_error(std::move(error));
        }
        m_ui_type = GncOptionUIType::INTERNAL;
    }
protected:
    OptionUIItem(GncOptionUIType ui_type) :
        m_ui_item{nullptr}, m_ui_type{ui_type} {}
    OptionUIItem(const OptionUIItem&) = default;
    OptionUIItem(OptionUIItem&&) = default;
    ~OptionUIItem() = default;
    OptionUIItem& operator=(const OptionUIItem&) = default;
    OptionUIItem& operator=(OptionUIItem&&) = default;
private:
    GncOptionUIItem* m_ui_item;
    GncOptionUIType m_ui_type;
};

template <typename ValueType>
class GncOptionValue :
    public OptionClassifier, public OptionUIItem
{
public:
    GncOptionValue<ValueType>(const char* section, const char* name,
                              const char* key, const char* doc_string,
                              ValueType value,
                              GncOptionUIType ui_type = GncOptionUIType::INTERNAL) :
        OptionClassifier{section, name, key, doc_string},
        OptionUIItem(ui_type),
        m_value{value}, m_default_value{value} {}
    GncOptionValue<ValueType>(const GncOptionValue<ValueType>&) = default;
    GncOptionValue<ValueType>(GncOptionValue<ValueType>&&) = default;
    GncOptionValue<ValueType>& operator=(const GncOptionValue<ValueType>&) = default;
    GncOptionValue<ValueType>& operator=(GncOptionValue<ValueType>&&) = default;
    ValueType get_value() const { return m_value; }
    ValueType get_default_value() const { return m_default_value; }
    void set_value(ValueType new_value) { m_value = new_value; }
private:
    ValueType m_value;
    ValueType m_default_value;
};

template <typename ValueType>
class GncOptionValidatedValue :
    public OptionClassifier, public OptionUIItem
{
public:
    GncOptionValidatedValue<ValueType>(const char* section, const char* name,
                                       const char* key, const char* doc_string,
                                       ValueType value,
                                       std::function<bool(ValueType)>validator,
                                       GncOptionUIType ui_type = GncOptionUIType::INTERNAL
        ) :
        OptionClassifier{section, name, key, doc_string},
        OptionUIItem(ui_type),
        m_value{value}, m_default_value{value}, m_validator{validator}
        {
            if (!this->validate(value))
            throw std::invalid_argument("Attempt to create GncValidatedOption with bad value.");
        }
    GncOptionValidatedValue<ValueType>(const char* section, const char* name,
                                       const char* key, const char* doc_string,
                                       ValueType value,
                                       std::function<bool(ValueType)>validator,
                                       ValueType val_data) :
        OptionClassifier{section, name, key, doc_string}, m_value{value},
        m_default_value{value}, m_validator{validator}, m_validation_data{val_data}
    {
            if (!this->validate(value))
            throw std::invalid_argument("Attempt to create GncValidatedOption with bad value.");
    }
    GncOptionValidatedValue<ValueType>(const GncOptionValidatedValue<ValueType>&) = default;
    GncOptionValidatedValue<ValueType>(GncOptionValidatedValue<ValueType>&&) = default;
    GncOptionValidatedValue<ValueType>& operator=(const GncOptionValidatedValue<ValueType>&) = default;
    GncOptionValidatedValue<ValueType>& operator=(GncOptionValidatedValue<ValueType>&&) = default;
    ValueType get_value() const { return m_value; }
    ValueType get_default_value() const { return m_default_value; }
    bool validate(ValueType value) { return m_validator(value); }
    void set_value(ValueType value)
    {
        if (this->validate(value))
            m_value = value;
        else
            throw std::invalid_argument("Validation failed, value not set.");
    }
private:
    ValueType m_value;
    ValueType m_default_value;
    std::function<bool(ValueType)> m_validator;                         //11
    ValueType m_validation_data;
};

/**
 * Used for numeric ranges and plot sizes.
 */

template <typename ValueType>
class GncOptionRangeValue :
    public OptionClassifier, public OptionUIItem
{
public:
    GncOptionRangeValue<ValueType>(const char* section, const char* name,
                                   const char* key, const char* doc_string,
                                   ValueType value, ValueType min,
                                   ValueType max, ValueType step) :
        OptionClassifier{section, name, key, doc_string},
        OptionUIItem(GncOptionUIType::NUMBER_RANGE),
        m_value{value >= min && value <= max ? value : min},
        m_default_value{value >= min && value <= max ? value : min},
        m_min{min}, m_max{max}, m_step{step} {}

    GncOptionRangeValue<ValueType>(const GncOptionRangeValue<ValueType>&) = default;
    GncOptionRangeValue<ValueType>(GncOptionRangeValue<ValueType>&&) = default;
    GncOptionRangeValue<ValueType>& operator=(const GncOptionRangeValue<ValueType>&) = default;
    GncOptionRangeValue<ValueType>& operator=(GncOptionRangeValue<ValueType>&&) = default;
    ValueType get_value() const { return m_value; }
    ValueType get_default_value() const { return m_default_value; }
    bool validate(ValueType value) { return value >= m_min && value <= m_max; }
    void set_value(ValueType value)
    {
        if (this->validate(value))
            m_value = value;
        else
            throw std::invalid_argument("Validation failed, value not set.");
    }
private:
    ValueType m_value;
    ValueType m_default_value;
    ValueType m_min;
    ValueType m_max;
    ValueType m_step;
};

/** MultiChoice options have a vector of valid options
 * (GncMultiChoiceOptionChoices) and validate the selection as being one of
 * those values. The value is the index of the selected item in the vector. The
 * tuple contains three strings, a key, a display
 * name and a brief description for the tooltip. Both name and description
 * should be localized at the point of use. 
 *
 *
 */
using GncMultiChoiceOptionEntry = std::tuple<const std::string,
                                             const std::string,
                                             const std::string>;
using GncMultiChoiceOptionChoices = std::vector<GncMultiChoiceOptionEntry>;

class GncOptionMultichoiceValue :
    public OptionClassifier, public OptionUIItem
{
public:
    GncOptionMultichoiceValue(const char* section, const char* name,
                              const char* key, const char* doc_string,
                              GncMultiChoiceOptionChoices&& choices,
                              GncOptionUIType ui_type = GncOptionUIType::MULTICHOICE) :
        OptionClassifier{section, name, key, doc_string},
        OptionUIItem(ui_type),
        m_value{}, m_default_value{}, m_choices{std::move(choices)} {}

    GncOptionMultichoiceValue(const GncOptionMultichoiceValue&) = default;
    GncOptionMultichoiceValue(GncOptionMultichoiceValue&&) = default;
    GncOptionMultichoiceValue& operator=(const GncOptionMultichoiceValue&) = default;
    GncOptionMultichoiceValue& operator=(GncOptionMultichoiceValue&&) = default;

    const std::string& get_value() const
    {
        return std::get<0>(m_choices.at(m_value));
    }
    const std::string& get_default_value() const
    {
        return std::get<0>(m_choices.at(m_default_value));
    }
     bool validate(const std::string& value) const noexcept
    {
        auto index = find_key(value);
        return index != std::numeric_limits<std::size_t>::max();

    }
    void set_value(const std::string& value)
    {
        auto index = find_key(value);
        if (index != std::numeric_limits<std::size_t>::max())
            m_value = index;
        else
            throw std::invalid_argument("Value not a valid choice.");

    }
    std::size_t num_permissible_values() const noexcept
    {
        return m_choices.size();
    }
    std::size_t permissible_value_index(const std::string& key) const noexcept
    {
            return find_key(key);
    }
    const std::string& permissible_value(std::size_t index) const
    {
        return std::get<0>(m_choices.at(index));
    }
    const std::string& permissible_value_name(std::size_t index) const
    {
        return std::get<1>(m_choices.at(index));
    }
    const std::string& permissible_value_description(std::size_t index) const
    {
        return std::get<2>(m_choices.at(index));
    }
private:
    std::size_t find_key (const std::string& key) const noexcept
    {
        auto iter = std::find_if(m_choices.begin(), m_choices.end(),
                              [key](auto choice) {
                                  return std::get<0>(choice) == key; });
        if (iter != m_choices.end())
            return iter - m_choices.begin();
        else
            return std::numeric_limits<std::size_t>::max();

    }
    std::size_t m_value;
    std::size_t m_default_value;
    GncMultiChoiceOptionChoices m_choices;
};

/** Date options
 * A legal date value is a pair of either  and a RelativeDatePeriod, the absolute flag and a time64, or for legacy purposes the absolute flag and a timespec.
 * The original design allowed custom RelativeDatePeriods, but that facility is unused so we'll go with compiled-in enums.

gnc-date-option-show-time? -- option_data[1]
gnc-date-option-get-subtype -- option_data[0]
gnc-date-option-value-type m_value
gnc-date-option-absolute-time 
gnc-date-option-relative-time
 */

enum class DateType
{
    ABSOLUTE,
    STARTING,
    ENDING,
};

enum class RelativeDatePeriod : int64_t
{
    TODAY,
    THIS_MONTH,
    PREV_MONTH,
    CURRENT_QUARTER,
    PREV_QUARTER,
    CAL_YEAR,
    PREV_YEAR,
    ACCOUNTING_PERIOD
};

using DateSetterValue = std::pair<DateType, int64_t>;
class GncOptionDateValue : public OptionClassifier, public OptionUIItem
{
public:
    GncOptionDateValue(const char* section, const char* name,
                              const char* key, const char* doc_string) :
        OptionClassifier{section, name, key, doc_string},
        OptionUIItem(GncOptionUIType::DATE),
        m_type{DateType::ABSOLUTE}, m_period{RelativeDatePeriod::TODAY},
        m_date{static_cast<time64>(GncDateTime())} {}
        GncOptionDateValue(const GncOptionDateValue&) = default;
        GncOptionDateValue(GncOptionDateValue&&) = default;
        GncOptionDateValue& operator=(const GncOptionDateValue&) = default;
        GncOptionDateValue& operator=(GncOptionDateValue&&) = default;
    time64 get_value() const;
    time64 get_default_value() const { return static_cast<time64>(GncDateTime()); }
    void set_value(DateSetterValue);
    void set_value(time64 time) {
        m_type = DateType::ABSOLUTE;
        m_period = RelativeDatePeriod::TODAY;
        m_date = time;
    }
private:
    DateType m_type;
    RelativeDatePeriod m_period;
    time64 m_date;
};

using GncOptionVariant = std::variant<GncOptionValue<std::string>,
                                      GncOptionValue<bool>,
                                      GncOptionValue<int64_t>,
                                      GncOptionValue<QofInstance*>,
                                      GncOptionValue<QofQuery*>,
                                      GncOptionValue<std::vector<GncGUID>>,
                                      GncOptionMultichoiceValue,
                                      GncOptionRangeValue<int>,
                                      GncOptionRangeValue<double>,
                                      GncOptionValidatedValue<QofInstance*>,
                                      GncOptionDateValue>;

class GncOption
{
public:
    template <typename OptionType>
    GncOption(OptionType option) : m_option{option} {}

    template <typename ValueType>
    GncOption(const char* section, const char* name,
              const char* key, const char* doc_string,
              ValueType value,
              GncOptionUIType ui_type = GncOptionUIType::INTERNAL) :
        m_option{GncOptionValue<ValueType> {
            section, name, key, doc_string, value, ui_type}} {}

    template <typename ValueType> ValueType get_value() const
    {
        return std::visit([](const auto& option)->ValueType {
                if constexpr (std::is_same_v<std::decay_t<decltype(option.get_value())>, std::decay_t<ValueType>>)
                    return option.get_value();
                return ValueType {};
            }, m_option);
    }

    template <typename ValueType> ValueType get_default_value() const
    {
        return std::visit([](const auto& option)->ValueType {
                if constexpr (std::is_same_v<std::decay_t<decltype(option.get_value())>, std::decay_t<ValueType>>)
                    return option.get_default_value();
                return ValueType {};
            }, m_option);

    }

    template <typename ValueType> void set_value(ValueType value)
    {
        std::visit([value](auto& option) {
                if constexpr (std::is_same_v<std::decay_t<decltype(option.get_value())>, std::decay_t<ValueType>>)
                                 option.set_value(value);
            }, m_option);
    }
    const std::string& get_section() const
    {
        return std::visit([](const auto& option)->const std::string& {
                return option.m_section;
            }, m_option);
    }
    const std::string& get_name() const
    {
        return std::visit([](const auto& option)->const std::string& {
                return option.m_name;
            }, m_option);
    }
    const std::string& get_key() const
    {
        return std::visit([](const auto& option)->const std::string& {
                return option.m_sort_tag;
            }, m_option);
    }
    const std::string& get_docstring() const
    {
          return std::visit([](const auto& option)->const std::string& {
                return option.m_doc_string;
              }, m_option);
    }
    void set_ui_item(GncOptionUIItem* ui_elem)
    {
        std::visit([ui_elem](auto& option) {
                option.set_ui_item(ui_elem);
            }, m_option);
    }
    const GncOptionUIType get_ui_type() const
    {
        return std::visit([](const auto& option)->GncOptionUIType {
                return option.get_ui_type();
            }, m_option);
    }
    GncOptionUIItem* const get_ui_item() const
    {
        return std::visit([](const auto& option)->GncOptionUIItem* {
                return option.get_ui_item();
            }, m_option);
    }
    void make_internal()
    {
        std::visit([](auto& option) {
                option.make_internal();
            }, m_option);
    }
    GncOptionVariant& _get_option() const { return m_option; }
private:
    mutable GncOptionVariant m_option;
};

#endif //GNC_OPTION_HPP_
