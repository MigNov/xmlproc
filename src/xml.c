//#define DEBUG_XML
#define HAVE_END_AUTODUMPS

#define ROOT_ELEMENT "//operation-processor-script"

#include "xml.h"
#include <libxml/parser.h>
#include <libxml/xpath.h>
#define HAVE_DUMPS

#ifdef DEBUG_XML
#define DPRINTF(fmt, ...) \
do { fprintf(stderr, "xml: " fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) \
do {} while(0)
#endif

int numSubst = 0;
int numVars  = 0;

void defParser(xmlDocPtr doc, xmlNodePtr node, int i);
void defClose(xmlDocPtr doc, int num);

tXmlSubst *xmlSubst;
tXmlSubst XML_SUBST_NONE = { 0 };

tXmlSubst findSubst(char *name) {
	int i, idx = -1;

	DPRINTF("%s = Name: %s\n", __FUNCTION__, name);

	for (i = 0; i < numSubst; i++)
		if (strcmp(xmlSubst[i].value, name)) {
			idx = i;
			break;
		}

	DPRINTF("%s = idx: %d\n", __FUNCTION__, idx);
	return (idx >= 0) ? xmlSubst[idx] : XML_SUBST_NONE;
}

/* Substitutions */
int substInit(xmlDocPtr doc, int num) {
	xmlSubst = (tXmlSubst *) malloc( num * sizeof(tXmlSubst) );
	if (xmlSubst == NULL)
		return 0;

	return 1;
}

void substParser(xmlDocPtr doc, xmlNodePtr node, int i) {
	char *val, *name;

	numSubst = i + 1;

	val = (char *)xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
	name = (char *)xmlGetProp(node, (xmlChar *)"name");

	xmlSubst[i].name = strdup(name);
	xmlSubst[i].value= strdup(val);

	free(name);
	free(val);
}

char *getVarType(int type) {
	switch (type) {
		case VTYPE_UINT64_T:	return "uint64_t";
					break;
		case VTYPE_FLAG:	return "flag";
					break;
		case VTYPE_INT: 	return "int";
					break;
		case VTYPE_CHAR:	return "char";
					break;
	}

	return "???";
}

#ifdef HAVE_DUMPS
void dumpSubstitutes() {
	int i;

	for (i = 0; i < numSubst; i++)
		printf("Substitute %d: name = %s, value = %s\n", i+1, xmlSubst[i].name, xmlSubst[i].value);
}

void dumpVars(int type) {
	int i, ix = 1;

	for (i = 0; i < numVars; i++)
		if ((type == -1) || (vars[i].type == type))
			printf("Variable %d: name = %s, value = %s, type = %s\n", ix++, vars[i].name, vars[i].value, getVarType(vars[i].type));

	ix = 1;
	for (i = 0; i < gNumVars; i++)
		if ((type == -1) || (gVars[i].type == type))
			printf("Global variable %d: name = %s, value = %s, type = %s\n", ix++, gVars[i].name, gVars[i].value, getVarType(gVars[i].type));
}

void dumpFunctionResults() {
	int i;

	for (i = 0; i < gFuncResCnt; i++) {
		if (gFuncResults[i].type == VTYPE_INT)
			printf("Function %d: name = %s (subval #%d), value = %d, type = int\n", i+1, gFuncResults[i].function,
				gFuncResults[i].subval, gFuncResults[i].iValue);
		else
		if (gFuncResults[i].type == VTYPE_UINT64_T)
			printf("Function %d: name = %s (subval #%d), value = 0x%" PRIx64 ", type = uint64_t\n",
				i+1, gFuncResults[i].function, gFuncResults[i].subval, gFuncResults[i].uValue);
		else
		if (gFuncResults[i].type == VTYPE_CHAR)
			printf("Function %d: name = %s (subval #%d), value = '%s', type = char\n",
				i+1, gFuncResults[i].function, gFuncResults[i].subval, gFuncResults[i].cValue);
	}
}

#ifdef HAVE_PCI
void dumpPCIDevices() {
	int i;

	for (i = 0; i < gPCIDevCnt; i++)
		if (gPCIDevs[i].valid)
			printf("PCI device %d: %s, BDF=%x:%x.%x (vendor_id = 0x%x, device_id = 0x%x)\n", i+1, gPCIDevs[i].name,
				gPCIDevs[i].dev->bus, gPCIDevs[i].dev->dev, gPCIDevs[i].dev->func, gPCIDevs[i].dev->vendor_id,
				gPCIDevs[i].dev->device_id );
		else
			printf("PCI device %d: %s is not valid\n", i+1, gPCIDevs[i].name);
}
#endif

#else
void dumpSubstitutes() { };
void dumpVars() { };
void dumpFunctionResults() { };

#ifdef HAVE_PCI
void dumpPCIDevices() { };
#endif

#endif

void freeVars(tVariables *vars) {
	return;

	int i;

	for (i = 0; i < numVars; i++) {
		free(vars[i].name);
		//free(vars[i].value);
	}

	free(vars);
	numVars = 0;
}

#ifdef HAVE_PCI
void closePCIDevices() {
	int i;

	for (i = 0; i < gPCIDevCnt; i++)
		if ((gPCIDevs[i].valid) && (gPCIDevs[i].dev != NULL)) {
			pci_free_device(gPCIDevs[i].dev);
			free(gPCIDevs[i].dev);
			gPCIDevs[i].valid = 0;
		}

	pci_dcleanup();
}
#endif

void resetVars(int type, char *val) {
	int i, redo = 0;

	for (i = 0; i < numVars; i++)
		if ((vars[i].type == type) &&
			((strstr(val, vars[i].name) != NULL) || (strcmp(val, "all") == 0)))
		{
			DPRINTF("Deleting variable %s (index %d)\n", vars[i].name, i);
			if (i < numVars) {
				vars[i] = vars[numVars-1];
				redo = 1;
			}
			numVars--;
			vars = realloc( vars, numVars * sizeof(tVariables) );
			if (redo)
				resetVars(type, val);
		}
}

int isVarGlobal(char *str) {
	int i;
	for (i = 0; i < numVars; i++)
		if ((vars[i].name != NULL) && (strcmp(vars[i].name, str) == 0))
			return 0;

	for (i = 0; i < gNumVars; i++)
		if ((gVars[i].name != NULL) && (strcmp(gVars[i].name, str) == 0))
			return 1;

	return -1;
}

char *getCharValue(char *str) {
	int i;

	for (i = 0; i < numVars; i++)
		if ((vars[i].name != NULL) && (strcmp(vars[i].name, str) == 0)
		    && (vars[i].type == VTYPE_CHAR) )
			return vars[i].value;

	return NULL;
}

char *getFunctionCharValue(char *name) {
	int i;

	for (i = 0; i < gFuncResCnt; i++) {
		if ((gFuncResults[i].function != NULL)
			&& (strcmp(gFuncResults[i].function, name) == 0)
			&& (gFuncResults[i].type == VTYPE_CHAR))
			return gFuncResults[i].cValue;
	}

	return NULL;
}

uint64_t getValue(char *str, int allowVar) {
	char *endptr;
	uint64_t val;
	int base = 10;
	int neg = 0;

	if (str == NULL)
		return UINT64_MAX;

	if (strncmp(str, "0x", 2) == 0)
		base = 16;

	errno = 0;
	val = strtoull(str, &endptr, base);
	if (((val == 0) || (errno != 0)) && (allowVar))  {
		int i;

		if (strcmp(str, "##cpu_model") == 0)
			return CPUGetModel(NULL);
		if (allowVar == 2) {
			for (i = 0; i < numSubst; i++)
				if (strcmp(xmlSubst[i].value, str) == 0)
					return getValue(xmlSubst[i].name, 0);
		}
		if ((strlen(str) > 0) && (str[0] == '!')) {
			int tmpI = -1;
			neg = 1;
			tmpI = *str++;
		}
		for (i = 0; i < numVars; i++)
			if ((vars[i].name != NULL) && (strcmp(vars[i].name, str) == 0)) {
				val = getValue(vars[i].value, 0);
				if (neg)
					val = (val ? 0 : 1);
				return val;
			}

		for (i = 0; i < gNumVars; i++)
			if ((gVars[i].name != NULL) && (strcmp(gVars[i].name, str) == 0)) {
				val = getValue(gVars[i].value, 0);
				if (neg)
					val = (val ? 0 : 1);
				return val;
			}
	}

	return val;
}

int getMaxComponent(char *name, int type) {
	int i, num = 0, max = 0;
	char *str;

	if (type == 2) {
		for (i = 0; i < gFuncResCnt; i++) {
			if ((gFuncResults[i].function != NULL) && ((strcmp(gFuncResults[i].function, name) == 0)
			    || (strstr(gFuncResults[i].function, name) != NULL))) {
				if (strcmp(gFuncResults[i].function, name) == 0)
					return 0;

				str = strdup(gFuncResults[i].function);
				str += strlen(name) + 1;
				num = getValue(str, 0);
				if (num > max)
					max = num;
			}
		}
	}
	else
	if (type == 1) {
#ifdef HAVE_PCI
		for (i = 0; i < gPCIDevCnt; i++)
			if ((gPCIDevs[i].name != NULL) && ((strcmp(gPCIDevs[i].name, name) == 0)
			    || (strstr(gPCIDevs[i].name, name) != NULL))) {
				if (strcmp(gPCIDevs[i].name, name) == 0)
					return 0;

				str = strdup(gPCIDevs[i].name);
				str += strlen(name) + 1;
				num = getValue(str, 0);
				if (num > max)
					max = num;
			}
#else
		max = 0;
#endif
	}
	else {
		for (i = 0; i < numVars; i++)
			if ((vars[i].name != NULL) && ((strcmp(vars[i].name, name) == 0)
			     || (strstr(vars[i].name, name) != NULL))) {
				if (strcmp(vars[i].name, name) == 0)
					return 0;

				str = strdup(vars[i].name);
				str += strlen(name) + 1;
				num = getValue(str, 0);
				if (num > max)
					max = num;
		}
	}

	return max;
}

uint64_t loadVariable(char *name, int reqType) {
	int i;

	for (i = 0; i < numVars; i++)
		if ((((reqType > 0) && (vars[i].type == reqType)) || (reqType == 0)) && (vars[i].name != NULL)
		    && (strcmp(vars[i].name, name) == 0))
			return getValue(vars[i].value, 0);

	return UINT64_MAX;
}

int setFlag(char *name, int set) {
	vars = realloc( vars, ( numVars + 1 ) * sizeof(tVariables) );
	if (vars == NULL)
		return 0;

	vars[numVars].name = strdup(name);
	if (set)
		vars[numVars].value = "1";
	else
		vars[numVars].value = "0";
	vars[numVars].type = VTYPE_FLAG;

	numVars++;
	return 1;

}

int getVarIndex(char *name, int global) {
	int i;

	if (global == 0) {
		for (i = 0; i < numVars; i++)
			if ((vars[i].name != NULL) && (strcmp(vars[i].name, name) == 0))
				return i;
	}
	else {
		for (i = 0; i < gNumVars; i++)
			if ((gVars[i].name != NULL) && (strcmp(gVars[i].name, name) == 0))
				return i;
	}

	return -1;
}

int getFunctionIndex(char *name, int subval) {
	int i;

	for (i = 0; i < gFuncResCnt; i++)
		if ((gFuncResults[i].function != NULL) && (strcmp(gFuncResults[i].function, name) == 0) && (subval == gFuncResults[i].subval))
			return i;

	return -1;
}

int setValue(char *name, uint64_t value, int type) {
	char tmp[128] = { 0 };
	int idx;

	idx = getVarIndex(name, 0);
	if (idx < 0) {
		vars = realloc( vars, ( numVars + 1 ) * sizeof(tVariables) );
		if (vars == NULL)
			return 0;
		idx = numVars;
		numVars++;
	}

	if (type == VTYPE_UINT64_T)
		snprintf(tmp, 128, "0x%" PRIx64, value);
	if (type == VTYPE_INT)
		snprintf(tmp, 128, "%d", (int)value);

	vars[idx].name = strdup(name);
	vars[idx].value = strdup(tmp);
	vars[idx].type = type;

	return 1;
}

int setCharValue(char *name, char *value) {
	int idx;

	idx = getVarIndex(name, 0);
	if (idx < 0) {
		vars = realloc( vars, ( numVars + 1 ) * sizeof(tVariables) );
		if (vars == NULL)
			return 0;
		idx = numVars;
		numVars++;
	}

	vars[idx].name = strdup(name);
	vars[idx].value = strdup(value);
	vars[idx].type = VTYPE_CHAR;

	return 1;
}

int setGlobalValue(char *name, uint64_t value, int type) {
	char tmp[128] = { 0 };
	int idx;

	idx = getVarIndex(name, 1);
	if (idx < 0) {
		gVars = realloc( gVars, ( gNumVars + 1 ) * sizeof(tVariables) );
		if (gVars == NULL)
			return 0;
		idx = gNumVars;
		gNumVars++;
	}

	if (type == VTYPE_UINT64_T)
		snprintf(tmp, 128, "0x%" PRIx64, value);
	if (type == VTYPE_INT)
		snprintf(tmp, 128, "%d", (int)value);

	gVars[idx].name = strdup(name);
	gVars[idx].value = strdup(tmp);
	gVars[idx].type = type;

	return 1;
}

uint64_t processOp(char *type, uint64_t var1, uint64_t var2, int negval) {
	uint64_t var = 0;

    if (negval == 0) {
		if (strcmp(type, "and") == 0)
			var = var1 & var2;
		else
		if (strcmp(type, "or") == 0)
			var = var1 | var2;
		else
		if (strcmp(type, "eq") == 0)
			var = (var1 == var2);
		else
		if (strcmp(type, "neq") == 0)
			var = (var1 != var2);
		else
		if (strcmp(type, "greater") == 0)
			var = (var1 > var2);
		else
		if (strcmp(type, "lower") == 0)
			var = (var1 < var2);
		else
		if (strcmp(type, "shr") == 0)
			var = var1 >> var2;
		else
		if (strcmp(type, "shl") == 0)
			var = var1 << var;
		else
		if (strcmp(type, "add") == 0)
			var = var1 + var2;
		else
		if (strcmp(type, "subtract") == 0)
			var = var1 - var2;
		else
		if (strcmp(type, "multiply") == 0)
			var = var1 * var2;
		else
		if (strcmp(type, "divide") == 0)
			var = var1 / var2;
		else
			printf("Error: Unimplemented type '%s'\n", type);
	}
	else
	{
		if (strcmp(type, "and") == 0)
			var = var1 & ~var2;
		else
		if (strcmp(type, "or") == 0)
			var = var1 | ~var2;
		else
		if (strcmp(type, "eq") == 0)
			var = (var1 == ~var2);
		else
		if (strcmp(type, "neq") == 0)
			var = (var1 != ~var2);
		else
		if (strcmp(type, "greater") == 0)
			var = (var1 > ~var2);
		else
		if (strcmp(type, "lower") == 0)
			var = (var1 < ~var2);
		else
		if (strcmp(type, "shr") == 0)
			var = var1 >> ~var2;
		else
		if (strcmp(type, "shl") == 0)
			var = var1 << ~var;
		else
		if (strcmp(type, "add") == 0)
			var = var1 + ~var2;
		else
		if (strcmp(type, "subtract") == 0)
			var = var1 - ~var2;
		else
		if (strcmp(type, "multiply") == 0)
			var = var1 * ~var2;
		else
		if (strcmp(type, "divide") == 0)
			var = var1 / ~var2;
		else
			printf("Error: Unimplemented type '%s'\n", type);
	}

	return var;
}

int getType(char *ret) {
	if (strcmp(ret, "int") == 0)
		return VTYPE_INT;
	if (strcmp(ret, "uint64_t") == 0)
		return VTYPE_UINT64_T;
	if (strcmp(ret, "char") == 0)
		return VTYPE_CHAR;

	return 0;
}

#ifdef HAVE_PCI
void readFromPCIDevicesAndSave(char *name, char *type, char *varName, uint64_t reg) {
	struct pci_dev *p = NULL;
	char name2[256] = { 0 }, *str;
	uint32_t val = 0;
	int i;

	if ((type != NULL) && (strlen(type) > 0) && (type[0] == '-'))
		type++;

	for (i = 0; i < gPCIDevCnt; i++)
		if (strcmp(gPCIDevs[i].name, name) == 0)
			p = gPCIDevs[i].dev;

	if (p == NULL) {
		snprintf(name2, 256, "%s/", name);
		for (i = 0; i < gPCIDevCnt; i++)
			if (strstr(gPCIDevs[i].name, name2) != NULL) {
				char tmp[256];
				int current = 0;
				str = strdup(gPCIDevs[i].name);
				str += strlen(name2);
				current = getValue(str, 0);

				if ((strcmp(type, "long") == 0) || (type == NULL))
					val = pci_read_long(gPCIDevs[i].dev, reg);
				else
				if (strcmp(type, "word") == 0)
					val = pci_read_word(gPCIDevs[i].dev, reg);
				else
				if (strcmp(type, "byte") == 0)
					val = pci_read_byte(gPCIDevs[i].dev, reg);

				snprintf(tmp, 256, "%s/%d", varName, current);
				setValue(tmp, val, VTYPE_UINT64_T);
			}
	}
}

int loadAndWriteToPCIDevices(char *name, char *type, char *varName, uint64_t reg) {
	struct pci_dev *p = NULL;
	char name2[256] = { 0 }, *str;
	uint64_t val;
	int i;

	if ((type != NULL) && (strlen(type) > 0) && (type[0] == '-'))
		type++;

	val = loadVariable(varName, 0);
	DPRINTF("Variable %s contents: 0x%" PRIx64"\n", varName, val);

	for (i = 0; i < gPCIDevCnt; i++)
		if (strcmp(gPCIDevs[i].name, name) == 0)
			p = gPCIDevs[i].dev;

	if (p == NULL) {
		snprintf(name2, 256, "%s/", name);
		for (i = 0; i < gPCIDevCnt; i++)
			if (strstr(gPCIDevs[i].name, name2) != NULL) {
				int current = 0;
				str = strdup(gPCIDevs[i].name);
				str += strlen(name2);
				current = getValue(str, 0);

				if ((strcmp(type, "long") == 0) || (type == NULL))
					return pci_write_long(gPCIDevs[i].dev, reg, (u32)val);
				else
				if (strcmp(type, "word") == 0)
					return pci_write_word(gPCIDevs[i].dev, reg, (u16)val);
				else
				if (strcmp(type, "byte") == 0)
					return pci_write_byte(gPCIDevs[i].dev, reg, (u8)val);
			}
	}

	return -ENOTSUP;
}

int getPCIDevExist(char *dev) {
	int i;

	if (gPCIDevCnt == 0)
		return 0;

	if (getMaxComponent(dev, 1) == 0) {
		for (i = 0; i < gPCIDevCnt; i++) {
			if ((strcmp(gPCIDevs[i].name, dev) == 0) && gPCIDevs[i].valid)
				return 1;
		}
	}

	for (i = 0; i < gPCIDevCnt; i++) {
		if ((strstr(gPCIDevs[i].name, dev) != NULL) && gPCIDevs[i].valid)
			return 1;
	}

	return 0;
}
#endif

int processBlock(xmlDocPtr doc, xmlNodePtr node, char *name, char *ret) {
	char *type, *val, *data;
	int isHavingMSR = 0;
	tXmlSubst xs;

	if (!ret)
		return 0;

	node = node->xmlChildrenNode;
	while (node != NULL) {
		data = (char *)xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
		type = (char *)xmlGetProp(node, (xmlChar *)"type");
		 val = (char *)xmlGetProp(node, (xmlChar *)"value");

		if (!xmlStrcmp(node->name, (const xmlChar *)"dev")) {
			int num = 0;
			char *token = NULL, *str = NULL, *saveptr1 = NULL;
			char *val1 = NULL, *val2 = NULL, *val3 = NULL;
			uint64_t var1 = 0, var2 = 0, var3 = 0;
#ifdef HAVE_PCI
			struct pci_dev **pcidev = NULL;
			int devNum = 0, i = 0;
#endif

			DPRINTF("Device data: %s, Type: %s, Val: %s\n", data, type, val);

			for (str = val; ; str = NULL) {
				token = strtok_r(str, ", ", &saveptr1);
				if (token == NULL)
					break;
				if (num == 0)
					val1 = strdup(token);
				else
				if (num == 1)
					val2 = strdup(token);
				else
				if (num == 2)
					val3 = strdup(token);
				else
					printf("Error: Value #%d is not supported\n", num);

				num++;
			}

			var1 = getValue(val1, 2);
			var2 = getValue(val2, 2);
			var3 = getValue(val3, 2);

			if (strcmp(type, "pci") == 0) {
#ifdef HAVE_PCI
				pcidev = pci_getDevices(&devNum, var1, var2, var3);

				if (devNum > 0) {
					if (gPCIDevs == NULL) {
						gPCIDevCnt = 0;
						gPCIDevs = (tPCIDevices *)malloc( devNum * sizeof(tPCIDevices) );
					} else
						gPCIDevs = (tPCIDevices *)realloc( gPCIDevs, (gPCIDevCnt + devNum) * sizeof(tPCIDevices) );

					while (i < devNum) {
						char devname[1024] = { 0 };
						snprintf(devname, 1024, "%s/%d", data, i+1);
						gPCIDevs[gPCIDevCnt].name = strdup(devname);
						gPCIDevs[gPCIDevCnt].valid = (pcidev[i] != NULL);
						gPCIDevs[gPCIDevCnt].dev = pcidev[i++];
						gPCIDevCnt++;
					}
				}
#else
				fprintf(stderr, "Error: PCI device support not compiled\n");
#endif
			}
			else
				fprintf(stderr, "Error: Unsupported device type\n");
		}
		else
		if (!xmlStrcmp(node->name, (const xmlChar *)"reg")) {
			DPRINTF("Register data: %s, Type: %s, Val: %s\n", data, type, val);

			if (strcmp(type, "msr") == 0) {
				uint64_t u64t;
				int err;

				xs = findSubst(val);
				err = MSRRead(0, getValue(xs.name, 0), &u64t);
				if (err == EMSROK)
					setValue(data, u64t, VTYPE_UINT64_T);
				else {
					fprintf(stderr, "Function %s: %s (MSR = 0x%x)\n",
						name, MSRGetError(err), (int)getValue(xs.name, 0));
					isHavingMSR = -1;
					break;
				}
				isHavingMSR++;
			}
			else
#ifdef HAVE_PCI
			if (strstr(type, "pci") != NULL) {
				char *str = NULL, *token = NULL, *saveptr1 = NULL;
				char *val1 = NULL, *val2 = NULL;
				int num = 0;

				for (str = val; ; str = NULL) {
					token = strtok_r(str, ", ", &saveptr1);
					if (token == NULL)
						break;
					if (num == 0)
						val1 = strdup(token);
					else
					if (num == 1)
						val2 = strdup(token);
					else
						printf("Error: Value #%d is not supported\n", num);

					num++;
				}

				readFromPCIDevicesAndSave(val1, type + 3, data, getValue(val2, 2));
			}
			else
#endif
			if (strcmp(type, "cpuid") == 0) {
				unsigned int regs[4], reg;
				char *str = NULL, *token = NULL, *saveptr1 = NULL, *vl = NULL;
				int num = 0, regIdx = -1;

				for (str = val; ; str = NULL) {
					token = strtok_r(str, ", ", &saveptr1);
					if (token == NULL)
						break;
					if (num == 0) {
						if (strcmp(token, "eax") == 0)
							regIdx = 0;
						else
						if (strcmp(token, "ebx") == 0)
							regIdx = 1;
						else
						if (strcmp(token, "ecx") == 0)
							regIdx = 2;
						else
						if (strcmp(token, "edx") == 0)
							regIdx = 3;
					}
					else
					if (num == 1)
						vl = strdup(token);
					else
						printf("Error: Value #%d is not supported\n", num);

					num++;
				}

				if (regIdx < 0) {
					free(vl);
					fprintf(stderr, "Function %s: Unknown register for cpuid. Valid values "
							"are: eax, ebx, ecx, edx\n", name);
					break;
				}

				CPUIDRead(0x00000000, regs);
				reg = (unsigned int)getValue(vl, 0);
				if (regs[0] < reg) {
					free(vl);
					fprintf(stderr, "Function %s: Cannot read cpuid (eax = %x)\n",
							name, reg);
					break;
				}

				CPUIDRead(reg, regs);
				DPRINTF("Saving value 0x%" PRIx64" to %s\n", (uint64_t)regs[regIdx], data);
				setValue(data, (uint64_t)regs[regIdx], VTYPE_UINT64_T);
			}
			else
				fprintf(stderr, "Register type %s is not supported!\n", type);
		}
		else
#ifdef HAVE_PCI
		if (!xmlStrcmp(node->name, (const xmlChar *)"write-reg")) {
			if (strstr(type, "pci") == 0) {
				char *str = NULL, *token = NULL, *saveptr1 = NULL;
				char *val1 = NULL, *val2 = NULL;
				int num = 0;

				for (str = val; ; str = NULL) {
					token = strtok_r(str, ", ", &saveptr1);
					if (token == NULL)
						break;
					if (num == 0)
						val1 = strdup(token);
					else
					if (num == 1)
						val2 = strdup(token);
					else
						printf("Error: Value #%d is not supported\n", num);

					num++;
				}

				loadAndWriteToPCIDevices(val1, type + 3, data, getValue(val2, 2));
			}
		}
		else
#endif
		if (!xmlStrcmp(node->name, (const xmlChar *)"condition-block")) {
			int num = 0;
			uint64_t var1 = 0, var2 = 0, var = 0;
			char *str = NULL, *saveptr1 = NULL, *token = NULL, *val1 = NULL, *val2 = NULL;

			DPRINTF("Condition block, condition type: %s, val: %s\n", type, val);

			for (str = val; ; str = NULL) {
				token = strtok_r(str, ", ", &saveptr1);
				if (token == NULL)
					break;
				if (num == 0)
					val1 = strdup(token);
				else
				if (num == 1)
					val2 = strdup(token);
				else
					printf("Error: Value #%d is not supported\n", num);

				num++;
			}

			var1 = loadVariable(val1, 0);
			if (var1 != UINT64_MAX) {
				int negval = 0, tmpI = -1;
				if ((val2 != NULL) && (strlen(val2) > 0))
					while (*val2 == ' ') tmpI = *val2++;
				if (val2[0] == '~') {
					negval = 1;
					tmpI = *val2++;
				}

				var2 = getValue(val2, 1);

				DPRINTF("Processing 0x%"PRIx64" %s 0x%"PRIx64" ...\n", var1, type, var2);

				var = processOp(type, var1, var2, negval);

				DPRINTF("Result is 0x%" PRIx64"\n", var);
				if (var > 0)
					processBlock(doc, node, "condition-block", ret);
			}

			free(val1);
			free(val2);
		}
		else
		if (!xmlStrcmp(node->name, (const xmlChar *)"op")) {
			int num = 0, max = 0;
			uint64_t var1 = 0, var2 = 0, var = 0;
			char *str = NULL, *saveptr1 = NULL, *token = NULL, *val1 = NULL, *val2 = NULL;

			DPRINTF("Operation data: %s, Type: %s, Val: %s\n", data, type, val);

			for (str = val; ; str = NULL) {
				token = strtok_r(str, ", ", &saveptr1);
				if (token == NULL)
					break;
				if (num == 0)
					val1 = strdup(token);
				else
				if (num == 1)
					val2 = strdup(token);
				else
					printf("Error: Value #%d is not supported\n", num);

				num++;
			}

			max = getMaxComponent(val1, 0);
			if (max == 0) {
				var1 = loadVariable(val1, 0);
				if (var1 != UINT64_MAX) {
					int negval = 0, tmpI = -1;
					if ((val2 != NULL) && (strlen(val2) > 0))
						while (*val2 == ' ') tmpI = *val2++;
					if (val2[0] == '~') {
						negval = 1;
						tmpI = *val2++;
					}

					var2 = getValue(val2, 1);

					DPRINTF("Processing 0x%"PRIx64" %s 0x%"PRIx64" ...\n", var1, type, var2);

					var = processOp(type, var1, var2, negval);

					DPRINTF("Saving value 0x%" PRIx64" into %s\n", var, data);
					setValue(data, var, VTYPE_UINT64_T);
				}
			}
			else {
				int i;
				char varName[256] = { 0 };

				for (i = 0; i < max; i++) {
					snprintf(varName, 256, "%s/%d", val1, i + 1);
					DPRINTF("Accessing %s\n", varName);
					var1 = loadVariable(varName, 0);
					snprintf(varName, 256, "%s/%d", data, i + 1);
					if (var1 != UINT64_MAX) {
						int negval = 0, tmpI = -1;
						if ((val2 != NULL) && (strlen(val2) > 0))
							while (*val2 == ' ') tmpI = *val2++;
						if (val2[0] == '~') {
							negval = 1;
							tmpI = *val2++;
							DPRINTF("Negation found for %s\n", val2);
						}
						var2 = getValue(val2, 1);
						DPRINTF("Processing 0x%"PRIx64" %s 0x%"PRIx64" ...\n", var1, type, var2);
						var = processOp(type, var1, var2, negval);
						DPRINTF("Saving value 0x%" PRIx64" into %s\n", var, varName);
						setValue(varName, var, VTYPE_UINT64_T);
					}
				}
			}
			free(val1);
		}
		else
		if (!xmlStrcmp(node->name, (const xmlChar *)"flagIf")) {
			int num = 0, negval = 0, tmpI = -1;
			uint64_t var = 0;
			char *str = NULL, *saveptr1 = NULL, *token = NULL, *val1 = NULL, *val2 = NULL;

			DPRINTF("Flag data: %s, Type: %s, Val: %s\n", data, type, val);

			for (str = val; ; str = NULL) {
				token = strtok_r(str, ", ", &saveptr1);
				if (token == NULL)
					break;
				if (num == 0)
					val1 = strdup(token);
				else
				if (num == 1)
					val2 = strdup(token);
				else
					printf("Error: Value #%d is not supported\n", num);

				num++;
			}

			if ((val2 != NULL) && (strlen(val2) > 0))
				while (*val2 == ' ') tmpI = *val2++;
			if (val2[0] == '~') {
				negval = 1;
				tmpI = *val2++;
			}

			var = processOp(type, getValue(val1, 1), getValue(val2, 1), negval);
			if (var)
				setFlag(data, 1);

			free(val1);
			free(val2);
		}
		else
		if (!xmlStrcmp(node->name, (const xmlChar *)"format")) {
			DPRINTF("Format type = %s, value = %s, data = %s\n", type, val, data);

			if ((strcmp(type, "string") == 0) || (strcmp(type, "string-and-free") == 0)) {
				char *str = NULL, *token = NULL, *saveptr = NULL;
				char tmp[16] = { 0 }, tmp2[32] = { 0 };
				char result[1024] = { 0 };
				int num = 0;
				uint64_t rval = 0;
				for (str = data; ; str = NULL) {
					token = strtok_r(str, "%%", &saveptr);
					if (token == NULL)
        					break;
					snprintf(tmp2, sizeof(tmp2), "%s%X", val, num + 1);
					rval = getValue( tmp2, 1 );
					memset(tmp2, 0, sizeof(tmp2));
					tmp2[0] = '%';
					strcat(tmp2, token);
					snprintf(tmp, sizeof(tmp), tmp2, (int)rval);
					strcat(result, tmp);
					num++;
				}

				setCharValue(val, result);

				if (strcmp(type, "string-and-free") == 0) {
					int i;
					char tmp[32] = { 0 };

					for (i = 0; i < num - 1; i++) {
						snprintf(tmp, sizeof(tmp), "%s%X", val, i + 1);
						resetVars(VTYPE_UINT64_T, tmp);
					}
				}
			}
		}
		else
		if (!xmlStrcmp(node->name, (const xmlChar *)"execute")) {
			DPRINTF("Execute %s, value = %s, variable = %s\n", type, val, data);

			if (strcmp(type, "function-char") == 0) {
				char tmp[256] = { 0 };
				snprintf(tmp, sizeof(tmp), "/definitions/function[@name='%s']", val);
				if (parseKeys(doc, ROOT_ELEMENT, tmp, defParser, NULL, defClose)) {
					dumpFunctionResults();
				}
				setCharValue(data, getFunctionCharValue(val));
			}
		}
		else
		if (!xmlStrcmp(node->name, (const xmlChar *)"dump")) {
			DPRINTF("Dump %s\n", type);

			#ifndef HAVE_DUMPS
			fprintf(stderr, "Error: Dumps are disabled!\n");
			break;
			#endif

			if (strcmp(type, "subst") == 0)
				dumpSubstitutes();
			else
			if (strcmp(type, "vars") == 0)
				dumpVars(-1);
			else
			if (strcmp(type, "flags") == 0)
				dumpVars(VTYPE_FLAG);
			else
			if (strcmp(type, "function-results") == 0)
				dumpFunctionResults();
			else
#ifdef HAVE_PCI
			if (strcmp(type, "pci-devices") == 0)
				dumpPCIDevices();
			else
#endif
				fprintf(stderr, "Error: Invalid dump type %s\n", type);
		}
		else
		if (!xmlStrcmp(node->name, (const xmlChar *)"reset")) {
			DPRINTF("Reset type: %s, Val: %s\n", type, val);

			if (strcmp(type, "flags") == 0)
				resetVars(VTYPE_FLAG, val);
			else
			if (strcmp(type, "uint64") == 0)
				resetVars(VTYPE_UINT64_T, val);
			else
			if (strcmp(type, "int") == 0)
				resetVars(VTYPE_INT, val);
		}
		else
		if (!xmlStrcmp(node->name, (const xmlChar *)"returnIf")) {
			int num = 0, t = 0;
			uint64_t var = 0;
			char *str = NULL, *saveptr1 = NULL, *token = NULL, *val1 = NULL, *val2 = NULL;

			DPRINTF("Conditional return data: %s, Type: %s, Val: %s\n", data, type, val);

			for (str = val; ; str = NULL) {
				token = strtok_r(str, ", ", &saveptr1);
				if (token == NULL)
					break;
				if (num == 0)
					val1 = strdup(token);
				else
				if (num == 1)
					val2 = strdup(token);
				else
					printf("Error: Value #%d is not supported\n", num);

				num++;
			}

#ifdef HAVE_PCI
			if (strcmp(val2, "NULL") == 0)
				var = processOp(type, getValue(val1, 1), getPCIDevExist(val1), 0);
			else
#endif
			{
				int negval = 0, tmpI = -1;
				if ((val2 != NULL) && (strlen(val2) > 0))
					while (*val2 == ' ') tmpI = *val2++;
				if (val2[0] == '~') {
					negval = 1;
					tmpI = *val2++;
				}

				var = processOp(type, getValue(val1, 1), getValue(val2, 1), negval);
			}

			if (var) {
				int idx;

				if (gFuncResults == NULL) {
					gFuncResCnt = 0;
					idx = 0;
					gFuncResults = (tFunctionResult *)malloc( sizeof(tFunctionResult) );
				}
				else {
					idx = getFunctionIndex(name, 0);
					if (idx < 0) {
						gFuncResults = (tFunctionResult *)realloc( gFuncResults, (gFuncResCnt + 1) * sizeof(tFunctionResult) );
						idx = gFuncResCnt;
						gFuncResCnt++;
					}
				}

				t = getType(ret);
				gFuncResults[idx].function = strdup(name);
				gFuncResults[idx].subval = 0;
				gFuncResults[idx].type = t;
				switch (t) {
					case VTYPE_UINT64_T:	gFuncResults[idx].uValue = getValue(data, 1);
								break;
					case VTYPE_INT:		gFuncResults[idx].iValue = (int)getValue(data, 1);
								break;
					case VTYPE_CHAR:	gFuncResults[idx].cValue = strdup(data);
								break;
					default:
								printf("Error: Unsupported return type '%s'\n", ret);
				}
				break;
			}

			free(val1);
			free(val2);
		}
		else
		if (!xmlStrcmp(node->name, (const xmlChar *)"setIf")) {
			int num = 0, negval = 0, tmpI = -1;
			uint64_t var = 0;
			char *str = NULL, *saveptr1 = NULL, *token = NULL, *val1 = NULL, *val2 = NULL;

			DPRINTF("Set data: %s, Type: %s, Val: %s\n", data, type, val);

			for (str = val; ; str = NULL) {
				token = strtok_r(str, ", ", &saveptr1);
				if (token == NULL)
					break;
				if (num == 0)
					val1 = strdup(token);
				else
				if (num == 1)
					val2 = strdup(token);
				else
					printf("Error: Value #%d is not supported\n", num);

				num++;
			}

			if ((val2 != NULL) && (strlen(val2) > 0))
				while (*val2 == ' ') tmpI = *val2++;
			if (val2[0] == '~') {
				negval = 1;
				tmpI = *val2++;
			}

			var = processOp(type, getValue(val1, 1), getValue(val2, 1), negval);
			free(val1);
			free(val2);
			if (var) {
				num = 0;
				for (str = data; ; str = NULL) {
					token = strtok_r(str, " = ", &saveptr1);
					if (token == NULL)
						break;
					if (num == 0)
						val1 = strdup(token);
					else
					if (num == 1)
						val2 = strdup(token);
					else
						printf("Error: Value #%d is not supported\n", num);

					num++;
				}

				setValue(val1, getValue(val2, 0), VTYPE_INT);

				free(val1);
				free(val2);
			}
		}
		else
		if (!xmlStrcmp(node->name, (const xmlChar *)"return")) {
			int i, t, max, idx, rq;
			char var2[256];

			DPRINTF("Return data are going to be saved to %s\n", val);

			max = getMaxComponent(val, 0);
			if (max == 0)
				max = 1;

			t = getType(ret);
			rq = 0;
			for (i = 0; i < max; i++) {
				if (max == 1)
					snprintf(var2, 256, "%s", val);
				else
					snprintf(var2, 256, "%s/%d", val, i + 1);

				idx = getFunctionIndex(name, (max == 1) ? 0 : i);
				if (idx < 0) {
					if (gFuncResults == NULL) {
						gFuncResCnt = 0;
						idx = 0;
						gFuncResults = (tFunctionResult *)malloc( sizeof(tFunctionResult) );
					}
					else {
						gFuncResults = (tFunctionResult *)realloc( gFuncResults, (gFuncResCnt + 1) * sizeof(tFunctionResult) );
						idx = gFuncResCnt;
						gFuncResCnt++;
					}
				}
				gFuncResults[idx].function = strdup(name);
				gFuncResults[idx].subval = ((max == 1) ? 0 : i);
				gFuncResults[idx].type = t;
				switch (t) {
					case VTYPE_UINT64_T:	gFuncResults[idx].uValue = getValue(var2, 1);
								break;
					case VTYPE_INT:		gFuncResults[idx].iValue = (int)getValue(var2, 1);
								break;
					case VTYPE_CHAR:	gFuncResults[idx].cValue = strdup(getCharValue(var2));
								break;
					default:
								printf("Error: Unsupported return type '%s'\n", ret);
				}
			}
			break;
		}
		else
		if (!xmlStrcmp(node->name, (const xmlChar *)"call")) {
			DPRINTF("Call node found. Data: %s, Type: %s, Val: %s\n", data, type, val);

			if (strcmp(type, "function") == 0) {
				int i;

				for (i = 0; i < gFuncResCnt; i++)
					if (strcmp(gFuncResults[i].function, val) == 0) {
						switch (gFuncResults[i].type) {
							case VTYPE_UINT64_T:	setValue(data, gFuncResults[i].uValue, gFuncResults[i].type);
										break;
							case VTYPE_INT:		setValue(data, gFuncResults[i].iValue, gFuncResults[i].type);
										break;
							case VTYPE_CHAR:	setCharValue(data, gFuncResults[i].cValue);
										break;
						}
					}
			}
			else
			if (strcmp(type, "print") == 0) {
				char msg[2048] = { 0 };
				int max;

				max = getMaxComponent(val, 0);
				if (max == 0) {
					if (strstr(data, "%s") != NULL)
						snprintf(msg, 2048, data, getCharValue(val) );
					else
						snprintf(msg, 2048, data, getValue(val, 1) );

					/* Print a message */
					printf("%s\n", msg);
				}
				else {
					int i;
					char varName[256];

					for (i = 0; i < max; i++) {
						snprintf(varName, 256, "%s/%d", val, i + 1);
						snprintf(msg, 2048, data, getValue(varName, 1) );
						printf("Value %d: %s\n", i + 1, msg);
					}
				}
			}
			else
				printf("Error: Invalid call type '%s'\n", val);
		}
		else
		if ((xmlStrcmp(node->name, (const xmlChar *)"text")) &&  (xmlStrcmp(node->name, (const xmlChar *)"comment")))
			printf("Unhandled node: %s\n", node->name);

		free(data);
		free(type);
		free(val);

		node = node->next;
	}

	return isHavingMSR;
}

int processFunction(xmlDocPtr doc, xmlNodePtr node) {
	char *name, *ret;
	int isHavingMSR;

	name = (char *)xmlGetProp(node, (xmlChar *)"name");
	ret  = (char *)xmlGetProp(node, (xmlChar *)"returnType");
	if (name)
		DPRINTF("Entering function '%s' (returns %s)\n", name, ret);

	numVars = 0;
	vars = (tVariables *)malloc( sizeof(tVariables) );

	isHavingMSR = processBlock(doc, node, name, ret);
	if (isHavingMSR == -1) {
		printf("Fatal error: Cannot process data\n");
		exit(1);
	}

#ifdef HAVE_END_AUTODUMPS
	if (name)
		printf("Variables for function %s:\n", name);

	if (numVars == 0)
		printf("No variables\n");
	dumpVars(-1);
#endif  

	free(ret);
	freeVars(vars);

	if (name)
		DPRINTF("Exiting from function %s\n", name);
	else
		DPRINTF("Finished processing\n");

	free(name);

	return 1;//(isHavingMSR == 0) ? 0 : 1;
}

/* Definitions */
void defParser(xmlDocPtr doc, xmlNodePtr node, int i) {
		char *name;
		name = (char *)xmlGetProp(node, (xmlChar *)"name");

		DPRINTF("Parsing definitions = { 'name' => '%s' }\n",
			name);

		node = node->xmlChildrenNode;
		while (node != NULL) {
			if (!xmlStrcmp(node->name, (const xmlChar *)"function")) {
				if (!processFunction(doc, node))
					break;
			}
			node = node->next;
		}

		free(name);
}

void defClose(xmlDocPtr doc, int num) {
#ifdef HAVE_PCI
	closePCIDevices();
#endif
}

int getIntValue(char *str)
{
	char *endptr = NULL;
	int tmp;

	errno = 0;
	tmp = strtol(str, &endptr, 10);
	if (errno != 0)
		return -1;
	return tmp;
}

uint32_t scriptGetVersion(xmlDocPtr doc, char *rootEl, int *major, int *minor, int *micro)
{
        int i, num;
        xmlXPathContextPtr context;
        xmlXPathObjectPtr op;
        xmlNodeSetPtr nodeset;
        char *tmp = NULL;
	uint32_t res = 0;

	context = xmlXPathNewContext(doc);
        if (context == NULL) {
                DPRINTF("Error in xmlXPathNewContext\n");
                return 0;
        }

        DPRINTF("Trying to access xPath node %s\n", rootEl);
        op = xmlXPathEvalExpression( (xmlChar *)rootEl, context);
        xmlXPathFreeContext(context);
        if (op == NULL) {
                DPRINTF("Error in xmlXPathEvalExpression\n");
                return 0;
        }
        if(xmlXPathNodeSetIsEmpty(op->nodesetval)){
                xmlXPathFreeObject(op);
                DPRINTF("No result\n");
                return 0;
        }

	nodeset = op->nodesetval;
	num = nodeset->nodeNr;
	for (i = 0; i < num; i++)
                tmp = (char *)xmlGetProp(nodeset->nodeTab[i], (xmlChar *)"version");

	char *str = NULL, *saveptr = NULL, *token = NULL;
	int tnum = 0, tval = -1;
	for (str = tmp; ; str = NULL) {
		token = strtok_r(str, ".", &saveptr);
		if (token == NULL)
			break;

		tval = getIntValue(token);
		if ((tnum > sizeof(res)) || (tval == -1)) {
			free(tmp);
			return 0;
		}

		if ((tnum == 0) && (major))
			*major = tval;
		if ((tnum == 1) && (minor))
			*minor = tval;
		if ((tnum == 2) && (micro))
			*micro = tval;

		res += tval << (tnum * 8);
		tnum++;
	}
	free(tmp);

	xmlXPathFreeObject(op);
	return res;
}

int scriptCheckVersion(uint32_t ver, int major, int minor, int micro)
{
	int vermajor, verminor, vermicro;

	vermajor = ver & 0xff;
	verminor = (ver >> 8) & 0xff;
	vermicro = (ver >> 16) & 0xff;

	return ((vermajor > major) || \
		((verminor == major) && (verminor > minor)) || \
		((vermajor == major) && (verminor == minor) && \
		(vermicro >= micro)));
}

/* Generic key parsing code */
int parseKeys(xmlDocPtr doc, char *rootEl, char *xpath,
		void (parser)(xmlDocPtr, xmlNodePtr, int),
		int (parserInit)(xmlDocPtr, int),
		void (parserFinish)(xmlDocPtr, int)) {

	int i, num;
	xmlXPathContextPtr context;
	xmlXPathObjectPtr op;
	xmlNodeSetPtr nodeset;
	char tmp[1024] = { 0 };

	if (parser == NULL) {
		DPRINTF("Parser is not set, parser have to be set\n");
		return 0;
	}
	
	context = xmlXPathNewContext(doc);
	if (context == NULL) {
		DPRINTF("Error in xmlXPathNewContext\n");
		return 0;
	}
	
	snprintf(tmp, sizeof(tmp), "%s%s", rootEl, xpath);
	DPRINTF("Trying to access xPath node %s\n", tmp);
	op = xmlXPathEvalExpression( (xmlChar *)tmp, context);
	xmlXPathFreeContext(context);
	if (op == NULL) {
		DPRINTF("Error in xmlXPathEvalExpression\n");
		return 0;
	}
	if(xmlXPathNodeSetIsEmpty(op->nodesetval)){
		xmlXPathFreeObject(op);
		DPRINTF("No result\n");
		return 0;
	}
	
	nodeset = op->nodesetval;
	num = nodeset->nodeNr;
	if (parserInit != NULL)
		if (!parserInit(doc, num)) {
			xmlXPathFreeObject(op);
			return 0;
		}
	
	for (i = 0; i < num; i++)
		parser(doc, nodeset->nodeTab[i], i);
	
	if (parserFinish != NULL)
		parserFinish(doc, num);
	
	xmlXPathFreeObject(op);
	
	return 1;
}

/* Actions */
int actionInit(xmlDocPtr doc, int i) {
	actionRun = 0;

	return 1;
}

void actionParser(xmlDocPtr doc, xmlNodePtr node, int unused) {
	//processBlock(doc, node, "entry", "int");
	processFunction(doc, node);
}

int processXml(char *xmlFile) {
	xmlDocPtr doc;
	int rc = 1;

	if (access(xmlFile, R_OK) != 0) {
		fprintf(stderr, "Error: File %s doesn't exist or is not accessible for reading.\n", xmlFile);
		return 0;
	}

	doc = xmlParseFile(xmlFile);

	if (!scriptCheckVersion(
		scriptGetVersion(doc, ROOT_ELEMENT, NULL, NULL, NULL), 0, 0, 1)) {
			DPRINTF("Error: Invalid version!\n");
			return -EINVAL;
		}

	gNumVars = 0;
	gVars = (tVariables *)malloc( sizeof(tVariables) );
	lastTime = 0;

	if (!parseKeys(doc, ROOT_ELEMENT, "/substitutions/subst", substParser, substInit, NULL)) {
		rc = 0;
		goto end;
	}

	if (!parseKeys(doc, ROOT_ELEMENT, "/definitions/function", defParser, NULL, defClose)) {
		rc = 0;
		goto end;
	}

	if (!parseKeys(doc, ROOT_ELEMENT, "/main-block", actionParser, actionInit, NULL)) {
		rc = 0;
		goto end;
	}

end:
	free(gVars);
	xmlFreeDoc(doc);
	xmlCleanupParser();

	return rc;
}

