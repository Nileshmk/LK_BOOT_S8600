#include <assert.h>
#include <bits.h>
#include <stdlib.h>
#include <string.h>
#include <dev/gpio.h>
#include <dev/ssbi.h>
#include <kernel/event.h>
#include <kernel/timer.h>
#include <reg.h>
#include <platform/iomap.h>
#include <platform/timer.h>
#include <platform.h>
#include <dev/pm8058.h>



extern int pa1_ssbi2_write_bytes(unsigned char *buffer, unsigned short length, unsigned short slave_addr);

extern int pa1_ssbi2_read_bytes(unsigned char *buffer, unsigned short length, unsigned short slave_addr);

extern int pa2_ssbi2_write_bytes(unsigned char *buffer, unsigned short length, unsigned short slave_addr);

extern int pa2_ssbi2_read_bytes(unsigned char *buffer, unsigned short length, unsigned short slave_addr);

/* PM8058 APIs */
int pm8058_write(uint16_t addr, uint8_t * data, uint16_t length)
{
	return pa1_ssbi2_write_bytes(data, length, addr);
}

int pm8058_read(uint16_t addr, uint8_t * data, uint16_t length)
{
	return pa1_ssbi2_read_bytes(data, length, addr);
}

int pm8058_get_irq_status(pm_irq_id_type irq, bool * rt_status)
{
	unsigned block_index, reg_data, reg_mask;
	int errFlag;

	block_index = PM_IRQ_ID_TO_BLOCK_INDEX(irq);

	/* select the irq block */
	errFlag = i2c_ssbi_write_bytes(&block_index, 1, IRQ_BLOCK_SEL_USR_ADDR);

	if (errFlag) {
		dprintf(INFO, "Device Timeout");
		return 1;
	}

	/* read real time status */
	errFlag = i2c_ssbi_read_bytes(&reg_data, 1, IRQ_STATUS_RT_USR_ADDR);
	if (errFlag) {
		dprintf(INFO, "Device Timeout");
		return 1;
	}
	reg_mask = PM_IRQ_ID_TO_BIT_MASK(irq);

	if ((reg_data & reg_mask) == reg_mask) {
		/* The RT Status is high. */
		*rt_status = TRUE;
	} else {
		/* The RT Status is low. */
		*rt_status = FALSE;
	}
	return 0;
}


bool pm8058_gpio_get(unsigned int gpio)
{
	pm_irq_id_type gpio_irq;
	bool status;
	int ret;

	gpio_irq = gpio + PM_GPIO01_CHGED_ST_IRQ_ID;
	ret = pm8058_get_irq_status(gpio_irq, &status);

	if (ret)
		dprintf(CRITICAL, "pm8058_gpio_get failed\n");

	return status;
}


int pm8058_mwrite(uint16_t addr, uint8_t val, uint8_t mask, uint8_t * reg_save)
{
	int rc = 0;
	uint8_t reg;

	reg = (*reg_save & ~mask) | (val & mask);
	if (reg != *reg_save)
		rc = pm8058_write(addr, &reg, 1);
	if (rc)
		dprintf(CRITICAL, "pm8058_write failed; addr=%03X, rc=%d\n",
			addr, rc);
	else
		*reg_save = reg;
	return rc;
}

#if 0
int pm8058_ldo_set_voltage()
{
	int ret = 0;
	unsigned vprog = 0x00000110;
	ret =
	    pm8058_mwrite(PM8058_HDMI_L16_CTRL, vprog, LDO_CTRL_VPROG_MASK, 0);
	if (ret) {
		dprintf(SPEW, "Failed to set voltage for l16 regulator\n");
	}
	return ret;
}

int pm8058_vreg_enable()
{
	int ret = 0;
	ret =
	    pm8058_mwrite(PM8058_HDMI_L16_CTRL, REGULATOR_EN_MASK,
			  REGULATOR_EN_MASK, 0);
	if (ret) {
		dprintf(SPEW, "Vreg enable failed for PM 8058\n");
	}
	return ret;
}
#endif

 
int pm8058_gpio_config(int gpio, struct pm8058_gpio *param)
{
	int	rc;
	unsigned char bank[8];

	static int dir_map[] = {
		PM8058_GPIO_MODE_OFF,
		PM8058_GPIO_MODE_OUTPUT,
		PM8058_GPIO_MODE_INPUT,
		PM8058_GPIO_MODE_BOTH,
	};

	if (param == 0) {
	  dprintf (INFO, "pm8058_gpio struct not defined\n");
          return -1;
	}

	/* Select banks and configure the gpio */
	bank[0] = PM8058_GPIO_WRITE |
		((param->vin_sel << PM8058_GPIO_VIN_SHIFT) &
			PM8058_GPIO_VIN_MASK) |
		PM8058_GPIO_MODE_ENABLE;

	bank[1] = PM8058_GPIO_WRITE |
		((1 << PM8058_GPIO_BANK_SHIFT) & PM8058_GPIO_BANK_MASK) |
		((dir_map[param->direction] << PM8058_GPIO_MODE_SHIFT) &
			PM8058_GPIO_MODE_MASK) |
		((param->direction & PM_GPIO_DIR_OUT) ?
			PM8058_GPIO_OUT_BUFFER : 0);

	bank[2] = PM8058_GPIO_WRITE |
		((2 << PM8058_GPIO_BANK_SHIFT) & PM8058_GPIO_BANK_MASK) |
		((param->pull << PM8058_GPIO_PULL_SHIFT) &
			PM8058_GPIO_PULL_MASK);

	bank[3] = PM8058_GPIO_WRITE |
		((3 << PM8058_GPIO_BANK_SHIFT) & PM8058_GPIO_BANK_MASK) |
		((param->out_strength << PM8058_GPIO_OUT_STRENGTH_SHIFT) &
			PM8058_GPIO_OUT_STRENGTH_MASK);

	bank[4] = PM8058_GPIO_WRITE |
		((4 << PM8058_GPIO_BANK_SHIFT) & PM8058_GPIO_BANK_MASK) |
		((param->function << PM8058_GPIO_FUNC_SHIFT) &
			PM8058_GPIO_FUNC_MASK);

	rc = i2c_ssbi_write_bytes(bank, 5, SSBI_REG_ADDR_GPIO(gpio));
	if (rc) {
	        dprintf(INFO, "Failed on 1st ssbi_write(): rc=%d.\n", rc);
		return 1;
	}
	return 0;
}
#if 0
int pm8058_gpio_config_kypd_drv(int gpio_start, int num_gpios, unsigned mach_id)
{
	int	rc;
	struct pm8058_gpio kypd_drv = {
		.direction	= PM_GPIO_DIR_OUT,
		.pull		= PM_GPIO_PULL_NO,
		.vin_sel	= 2,
		.out_strength	= PM_GPIO_STRENGTH_LOW,
		.function	= PM_GPIO_FUNC_1,
		.inv_int_pol	= 1,
	};

	if(mach_id == LINUX_MACHTYPE_8660_QT)
		kypd_drv.function = PM_GPIO_FUNC_NORMAL;

	while (num_gpios--) {
		rc = pm8058_gpio_config(gpio_start++, &kypd_drv);
		if (rc) {
		        dprintf(INFO, "FAIL pm8058_gpio_config(): rc=%d.\n", rc);
			return rc;
		}
	}

	return 0;
}

int pm8058_gpio_config_kypd_sns(int gpio_start, int num_gpios)
{
	int	rc;
	struct pm8058_gpio kypd_sns = {
		.direction	= PM_GPIO_DIR_IN,
		.pull		= PM_GPIO_PULL_UP1,
		.vin_sel	= 2,
		.out_strength	= PM_GPIO_STRENGTH_NO,
		.function	= PM_GPIO_FUNC_NORMAL,
		.inv_int_pol	= 1,
	};

	while (num_gpios--) {
		rc = pm8058_gpio_config(gpio_start++, &kypd_sns);
		if (rc) {
		        dprintf(INFO, "FAIL pm8058_gpio_config(): rc=%d.\n", rc);
			return rc;
		}
	}

	return 0;
}

#endif


void pmic_write(unsigned address, unsigned data)
{
	int rc;
	rc = i2c_ssbi_write_bytes(&data, 1, address);
	if (rc)
	{
		dprintf (CRITICAL, "Error to pmic write\n");
		return rc;
	}
	return 0;
}

